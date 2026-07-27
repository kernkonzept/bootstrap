#! /usr/bin/env perl
#
# Copyright (C) 2008-2009 Technische Universität Dresden.
# Author(s): Adam Lackorzynski <adam@os.inf.tu-dresden.de>
#
# License: see LICENSE.spdx (in this directory or the directories above)

use strict;
use warnings;
use feature qw/state/;

BEGIN { unshift @INC, $ENV{L4DIR}.'/tool/lib'
           if $ENV{L4DIR} && -d $ENV{L4DIR}.'/tool/lib/L4';}

use Digest::MD5;
use File::Basename;
use File::Path qw(make_path);
use POSIX;
use Math::BigInt;
use L4::ModList;
use L4::Image;
use L4::Image::Raw;

my $cross_compile_prefix = $ENV{CROSS_COMPILE} || '';
my $arch                 = $ENV{OPT_ARCH}     || "x86";
my $platform_type        = $ENV{OPT_PLATFORM_TYPE};
my $bits                 = $ENV{OPT_BITS};

my $module_path    = $ENV{SEARCHPATH}     || ".";
my $prog_objcopy   = $ENV{OBJCOPY}        || "${cross_compile_prefix}objcopy";
my $prog_cc        = $ENV{CC}             || "${cross_compile_prefix}gcc";
my $prog_nm        = $ENV{NM}             || "${cross_compile_prefix}nm";
my $prog_cp        = $ENV{PROG_CP}        || "cp";
my $prog_gzip      = $ENV{PROG_GZIP}      || "gzip";
my $compress       = $ENV{COMPRESS}       || 0;
my $can_decompress = $ENV{CAN_DECOMPRESS} || 0;
my $strip          = $ENV{OPT_STRIP}      || 1;
my $output_dir     = $ENV{OUTPUT_DIR}     || '.';
my $make_inc_file  = $ENV{MAKE_INC_FILE}  || "mod.make.inc";
my $flags_cc       = $ENV{FLAGS_CC}       || '';

sub usage()
{
  print STDERR "$0 modulefile entry\n";
}

sub error
{
  L4::Image::error(@_);
}

# Write a string to a file, overwriting it.
# 1:    filename
# 2..n: Strings to write to the file
sub write_to_file
{
  my $file = shift;

  open(A, ">$file") || die "Cannot open '$file': $!";
  while ($_ = shift) {
    print A;
  }
  close A;
}

sub round_kb($)
{
  return ($_[0] + 1023) / 1024;
}

sub default_mod_merge_text(%)
{
  my %d = @_;
  my $size_str = '';

  $size_str .= sprintf " =s> %d KB", round_kb($d{size_stripped})
    if $d{size_stripped};
  $size_str .= sprintf " =c> %d KB", round_kb($d{size_compressed})
    if $d{size_compressed};
  my $nostrip_str = $d{nostrip} ? " (not stripped)" : "";

  print "$d{modname}: $d{path} ",
        "[".int(round_kb($d{size_orig}))."KB$size_str]$nostrip_str\n";
}

sub default_output_begin
{
  my %entry = @_;
  print "Merging images:\n" if exists $entry{mods};
}

sub default_output_end
{}

my %output_formatter = (
  begin  => \&default_output_begin,
  module => \&default_mod_merge_text,
  end    => \&default_output_end,
);

# build object files from the modules
sub build_obj
{
  my ($file, $cmdline, $modname, $flags, $opts) = @_;
  my %d;

  $d{path} = L4::ModList::search_file($file, $module_path)
    || die "Cannot find file $file! Used search path: $module_path";

  if (exists $opts->{uncompress})
    {
      system("$prog_gzip -dc $d{path} > $modname.ugz 2> /dev/null");
      if ($?)
        {
          unlink("$modname.ugz");
        }
      else
        {
          $d{path} = "$modname.ugz";
        }
    }
  $d{size_orig} = -s $d{path};

  my $take_orig = 1;
  if ($strip and not exists $opts->{nostrip})
    {
      open(my $fd, '<:raw', "$d{path}") || die("Could not open '$d{path}': $!");

      my $buf;
      my $r = sysread($fd, $buf, 0x3c);
      die "Unable to read $d{path}: $!" if !defined $r;
      close($fd);

      if ($r == 0x3c && substr($buf, 0x38, 4) eq "\x41\x52\x4d\x64")
        {
          # Linux AARCH64 kernel image. 'objcopy -S' would remove the AArch64
          # header. The resulting PE image format isn't supported by Uvmm.
          $opts->{nostrip} = 1;
        }
      else
        {
          system("$prog_objcopy -S $d{path} $modname.obj 2> /dev/null");
          $take_orig = $?;
          if ($take_orig == 0)
            {
              $d{size_stripped} = -s "$modname.obj";
              undef $d{size_stripped} if $d{size_orig} == $d{size_stripped};
            }
        }
    }

  system("$prog_cp $d{path} $modname.obj") if $take_orig;

  $opts->{compress} = undef if $compress;

  state $warned_decompress = 0;
  unless ($can_decompress or not exists $opts->{compress} or $warned_decompress)
    {
      print("WARNING: bootstrap cannot decompress. Image will likely not boot.\n");
      $warned_decompress = 1;
    }
  my %imgmod = L4::Image::fill_module("$modname.obj", $opts,
                                       basename((split(/\s+/, $cmdline))[0]),
                                       $flags, $cmdline);
  error($imgmod{error}) if $imgmod{error};

  $d{modname} = $modname;

  &{$output_formatter{module}}(%d);

  return %imgmod;
}

sub build_objects(@)
{
  my %entry = @_;
  my @mods;
  @mods = @{$entry{mods}} if exists $entry{mods};
  my $obj_fn = "$output_dir/mbi_modules.o";
  
  unlink($make_inc_file);

  # generate module-names
  for (my $i = 0; $i < @mods; $i++) {
    $mods[$i]->{modname} = sprintf "mod%02d", $i;
  }

  my %img;

  if ($ENV{BOOTSTRAP_BUILD_OUTPUT_FORMATTER})
    {
      my $f = $ENV{BOOTSTRAP_BUILD_OUTPUT_FORMATTER};
      die "No such file '$f'" unless -e $f;
      my $n = do $f;
      die "Error: $@" if $@;
      die "Could not do file '$f': $!" unless defined $n;
      my %n = %$n;
      $output_formatter{$_} = $n{$_} foreach (keys %n);
    }

  &{$output_formatter{begin}}(%entry);

  for (my $i = 0; $i < @mods; $i++) {
    $img{mods}[$i] =
      { build_obj($mods[$i]->{file}, $mods[$i]->{cmdline},
                  $mods[$i]->{modname}, $mods[$i]->{type},
                  $mods[$i]->{opts}) };
  }

  &{$output_formatter{end}}();

  my $make_inc_str = "MODULE_OBJECT_FILES += $obj_fn\n".
                     "LDFLAGS             += -u _binary_modules_start\n";
  $make_inc_str   .= "MOD_ADDR            := $entry{modaddr}"
    if defined $entry{modaddr};

  write_to_file($make_inc_file, $make_inc_str);

  $img{num_mods}    = scalar @mods;
  $img{mbi_cmdline} = $entry{bootstrap}{cmdline} if @mods;
  $img{arch}        = $arch;

  $img{attrs}{"l4i:PT"} = $platform_type if defined $platform_type;
  $img{attrs}{"l4i:bits"} = $bits if defined $bits;
  $img{attrs}{"l4i:QEMUcmd"} = "$ENV{QEMU_BINARY_NAME} -kernel \$L4IMAGE_FILE $ENV{QEMU_OPTIONS}"
    if $ENV{QEMU_BINARY_NAME};

  $img{attrs}{"l4i:info-url"} = 'https://l4re.org';
  $img{attrs}{"l4i:info-url"} = $ENV{L4IMAGE_INFO_URL}
    if $ENV{L4IMAGE_INFO_URL};
  $img{attrs}{"l4i:license"} = $ENV{L4IMAGE_LICENSE}
    if $ENV{L4IMAGE_LICENSE};

  $img{attrs}{"l4i:loadaddr"} = $ENV{BOOTSTRAP_LINKADDR};
  $img{attrs}{"l4i:rambase"} = $ENV{OPT_RAM_BASE};
  $img{attrs}{"l4i:uefi"} = $ENV{OPT_EFIMODE};
  $img{attrs}{"bootstrap:features"} = "compress-gz" if $can_decompress;

  my $volatile_data = 1;

  if ($volatile_data)
    {
      my %a = L4::Image::creation_info("creation");
      $img{attrs}{$_} = $a{$_} foreach keys %a;
    }

  open(my $fd, ">mods.bin") || die "Cannot open 'mods.bin': $!";
  binmode $fd;
  my %offsets = L4::Image::export_modules($fd, %img);
  close $fd;

  open($fd, ">mods.offsets") || die "Cannot open 'mods.offsets': $!";
  print $fd "{\n";
  print $fd "  attrs => $offsets{attrs},\n" if defined $offsets{attrs};
  print $fd "  mod_header => $offsets{mod_header},\n" if defined $offsets{mod_header};
  print $fd "};\n";
  close $fd;

  my $section_attr = ($arch ne 'arm'
       ? '\"a\", @progbits'
       : '\"a\"' );

  my $alignment = 12;
  $alignment = 16 if $arch eq 'mips';

  write_to_file("mods.c",qq|
    asm (".section .module_data, $section_attr           \\n"
         ".p2align $alignment                            \\n"
         ".global _binary_modules_start                  \\n"
         "_binary_modules_start:                         \\n"
         ".incbin \\"mods.bin\\"                         \\n"
         "_binary_modules_end:                           \\n");
  |);

  system("$prog_cc $flags_cc -Wall -W -c -o $obj_fn mods.c");
  die "Compiling mods.obj failed" if $?;
  unlink("mods.c");
  unlink("mods.bin");
  unlink($_->{modname}.".obj") foreach @mods;
  unlink($_->{modname}.".ugz") foreach @mods;
}

sub get_files
{
  my %entry = @_;
  map { L4::ModList::search_file_or_die($_->{file}, $module_path) }
      @{$entry{mods}};
}

sub list_files
{
  print join(' ', get_files(@_)), "\n";
}

sub list_files_unique
{
  my %d;
  $d{$_} = 1 foreach (get_files(@_));
  print join(' ', keys %d), "\n";
}

sub fetch_files
{
  my %entry = @_;
  L4::ModList::fetch_remote_file($_->{file})
    foreach (@{$entry{mods}});
}

sub dump_entry(@)
{
  my %entry = @_;
  print "modaddr=$entry{modaddr}\n" if defined $entry{modaddr};
  print "$entry{bootstrap}{file}\n";
  print "$entry{bootstrap}{cmdline}\n";
  print join("\n", sort map { $_->{cmdline} } @{$entry{mods}}), "\n";
  print join(' ', sort map { $_->{file}.$_->{type} } @{$entry{mods}}), "\n";
}

sub postprocess
{
  my $fn = shift;
  error("Invalid '$0 postprocess' use, no file given") unless defined $fn;

  my $v = 0;

  my ($type, $err) = L4::Image::get_file_type($fn);
  error("Error detecting file: $err") if $err;

  print "Type: ", (L4::Image::FILE_TYPES)[$type], "\n" if $v;

  error("Need a bootstrap ELF file") unless $type == L4::Image::FILE_TYPE_ELF;

  my $is32bit = (L4::Image::Elf->new($fn)->{class} == L4::Image::Elf::ELFCLASS32);

  my $magic = L4::Image::dsi('BOOTSTRAP_IMAGE_INFO_MAGIC');
  my $count = `grep -c "$magic" $fn`;
  chomp $count;
  error("Multiple or no image info headers found -- must not be") if $count != 1;

  my $fn_nm = $fn;
  my ($symbol_image_info, $symbol_module_data_start, $symbol_end_of_initial_bootstrap);
  my $restart_nm;
  do
    {
      $restart_nm = 0;
      open(my $nm, "LC_ALL=C $prog_nm $fn_nm 2>&1 |") || error("Cannot get symbols from '$fn_nm'");
      while (<$nm>)
        {
          chomp;
          if (/: no symbols/ && $fn eq $fn_nm)
            {
              $fn_nm = dirname($fn).'/.debug/'.basename($fn).'.debug';
              error("No file with symbols found for '$fn'") unless -e $fn_nm;
              $restart_nm = 1;
              last;
            }
          $symbol_image_info        = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+[ABDTNR]\s+image_info$/i;
          $symbol_module_data_start = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+[BDTNR]\s+_module_data_start$/i;
          $symbol_end_of_initial_bootstrap = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+[BDTNR]\s+_end_of_initial_bootstrap$/i;
        }
      close $nm;
    }
  while ($restart_nm);

  error("Did not find _module_data_start symbol in binary")
    unless defined $symbol_module_data_start;

  error("Did not find image_info symbol in binary")
    unless defined $symbol_image_info;

  my $compensate_nm_bug = sub {
    # In some binutils distributions nm has a bug where it sign-extends 32-bit
    # addresses to 64-bit addresses before printing, which can lead to incorrect
    # addresses in the mod header. To compensate for this, we cut down the
    # addresses to the lower 32 bits if we're processing a 32-bit image.
    my $adr = shift;
    $adr = $adr->band(Math::BigInt->from_hex("FFFFFFFF")) if $is32bit;
    return $adr;
  };

  $symbol_image_info               = $compensate_nm_bug->($symbol_image_info);
  $symbol_module_data_start        = $compensate_nm_bug->($symbol_module_data_start);
  $symbol_end_of_initial_bootstrap = $compensate_nm_bug->($symbol_end_of_initial_bootstrap)
    if defined $symbol_end_of_initial_bootstrap;

  # module_data_start needs to be relative to &image_info
  my $new_module_data_start = $symbol_module_data_start - $symbol_image_info;

  # module_data_start needs to be relative to &image_info
  my $new_module_data_end = $symbol_end_of_initial_bootstrap - $symbol_image_info;

  open(my $fd, "+<$fn") || error("Could not open '$fn': $!");
  binmode $fd;

  my $buf;
  my $r = sysread($fd, $buf, 1 << 20); # would we be interrupted?
  error("Could not read from file") unless $r;

  my $pos = index($buf, L4::Image::dsi('BOOTSTRAP_IMAGE_INFO_MAGIC'));
  error("Did not find image info") if $pos == -1;

  if ($v)
    {
      printf "ELF: Filling image_info data at ELF-file pos 0x%x\n", $pos;
      print "ELF: _image_info=", $symbol_image_info->as_hex(), "\n";
      print "ELF: _module_data_start=", $symbol_module_data_start->as_hex(), "\n";
      print "ELF: _end_of_initial_bootstrap=", $symbol_end_of_initial_bootstrap->as_hex(), "\n";
    }

  $pos += L4::Image::dsi('BOOTSTRAP_IMAGE_INFO_MAGIC_LEN'); # jump over magic

  my ($_crc32, $_version, $_flags,
      $orig_module_data_start, $orig_module_data_end, $orig_module_header, $orig_attrs)
    = unpack(L4::Image::dsi('TEMPLATE_IMAGE_INFO'),
             substr($buf, $pos, L4::Image::dsi('IMAGE_INFO_SIZE')));

  if ($v)
    {
      print  "Original values in image:\n";
      print  " crc32=$_crc32\n";
      print  " version=$_version\n";
      print  " flags=$_flags\n";
      printf " module_data_start=%x\n", $orig_module_data_start;
      printf " module_data_end=%x\n", $orig_module_data_end;
      printf " module_header=%x\n", $orig_module_header;
      printf " attrs=%x\n", $orig_attrs;
    }

  my $fn_offs = dirname($fn)."/mods.offsets";
  my $offsets = do $fn_offs;
  unless ($offsets)
    {
      error "Coulnd't parse $fn_offs: $@" if $@;
      error "Coulnd't do $fn_offs: $!" unless defined $offsets;
      error "Coulnd't run $fn_offs: $!" unless $offsets;
    }

  my $new_module_header = 0;
  if (defined $offsets->{mod_header})
    {
      $new_module_header = $new_module_data_start + $offsets->{mod_header};
      printf "_mod_header=%x\n", $new_module_header if $v;
    }

  my $new_attrs = 0;
  if (defined $offsets->{attrs})
    {
      $new_attrs = $new_module_data_start + $offsets->{attrs};
      printf "_attrs=%x\n", $new_attrs if $v;
    }

  sysseek($fd, $pos, 0);
  $r = syswrite($fd, pack(L4::Image::dsi('TEMPLATE_IMAGE_INFO'),
                          0, # crc32
                          $_version,
                          $_flags,
                          $new_module_data_start,
                          $new_module_data_end,
                          $new_module_header,
                          $new_attrs),
                L4::Image::dsi('IMAGE_INFO_SIZE'));
  error("Could not patch binary")
    if not defined $r or $r != L4::Image::dsi('IMAGE_INFO_SIZE');

  # TODO: update crc32

  close $fd;
}

sub postprocess_raw
{
  my ($arch, $rawimage) = @_;

  open(my $fd, "+<:raw", $rawimage)
    or die "Unable to open $rawimage";

  L4::Image::Raw::patch_vmlinuz_header($fd);

  close($fd);
}

# ------------------------------------------------------------------------

my $cmd = $ARGV[0];

if (not defined $cmd)
  {
    print STDERR "Error: No command given!\n";
    exit 1;
  }

if ($cmd eq 'postprocess')
  {
    postprocess($ARGV[1]);
    exit(0);
  }
elsif ($cmd eq 'postprocess_raw')
  {
    postprocess_raw(@ARGV[1..2]);
    exit(0);
  }

my $modulesfile   = $ARGV[1];
my $entryname     = $ARGV[2];

if ($cmd ne 'build' and !$entryname) {
  print STDERR "Error: Invalid usage!\n";
  usage();
  exit 1;
}

if (not -d $output_dir)
  {
    make_path $output_dir || die "Cannot create '$output_dir': $!";
  }
chdir $output_dir || die "Cannot change to directory '$output_dir': $!";

my %entry = ( mbi_cmdline => '' );

%entry = L4::ModList::get_module_entry($modulesfile, $entryname, $module_path)
  if $modulesfile and $entryname;

if ($cmd eq 'build')
  {
    build_objects(%entry);
  }
elsif ($cmd eq 'list')
  {
    list_files(%entry);
  }
elsif ($cmd eq 'list_unique')
  {
    list_files_unique(%entry);
  }
elsif ($cmd eq 'fetch_files_and_list_unique')
  {
    fetch_files(%entry);
    list_files_unique(%entry);
  }
elsif ($cmd eq 'dump')
  {
    dump_entry(%entry);
  }
