#!/usr/bin/perl -w

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash, the Principle of Least Authority Shell.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.


use IO::File;

my $methods =
  [['Call', 'call'],

   ['Okay', 'okay'],
   ['Fail', 'fail', Args => 'errno_val/int'],

   ['Copy', 'fsop_copy'],
   ['Gdir', 'fsop_get_dir'],
   ['Grtd', 'fsop_get_root_dir'],
   ['Gobj', 'fsop_get_obj'],
   ['Logm', 'fsop_log'],
   # These correspond to Unix system calls:
   ['Open', 'fsop_open'],
     ['ROpn', 'r_fsop_open'],
     ['RDfd', 'r_fsop_open_dir'],
   ['Stat', 'fsop_stat'],
     ['RSta', 'r_fsop_stat'],
   ['Rdlk', 'fsop_readlink'],
     ['RRdl', 'r_fsop_readlink'],
   ['Chdr', 'fsop_chdir'],
   ['Fchd', 'fsop_fchdir'],
   ['Dfst', 'fsop_dir_fstat'], # fstat on directory FD objects only
   ['Gcwd', 'fsop_getcwd'],
     ['RCwd', 'r_fsop_getcwd'],
   ['Dlst', 'fsop_dirlist'],
     ['RDls', 'r_fsop_dirlist'],
   ['Accs', 'fsop_access'],
   ['Mkdr', 'fsop_mkdir'],
   ['Chmd', 'fsop_chmod'],
   ['Utim', 'fsop_utime'],
   ['Renm', 'fsop_rename'],
   ['Link', 'fsop_link'],
   ['Syml', 'fsop_symlink'],
   ['Unlk', 'fsop_unlink'],
   ['Rmdr', 'fsop_rmdir'],
   ['Fcon', 'fsop_connect'],
   ['Fbnd', 'fsop_bind'],
   ['Exec', 'fsop_exec'],

   # File, directory and symlink objects:
   ['Otyp', 'fsobj_type',
       Args => '',
       Result_code => 'ROty',
       Result => 'type/int'],
   ['Osta', 'fsobj_stat',
       Args => '',
       Result_code => 'ROst'],
   ['Outm', 'fsobj_utimes'],
   ['Ochm', 'fsobj_chmod',
       Args => 'mode/int',
       Result => ''],
   # File objects:
   ['Oopn', 'file_open',
       Args => 'flags/int',
       Result_code => 'ROop',
       Result => 'fd'],
   ['Ocon', 'file_socket_connect',
       Args => 'sock/fd',
       Result => ''],
   # Directory objects:
   ['Otra', 'dir_traverse',
       Args => 'leaf/string',
       Result_code => 'ROtr',
       Result => 'obj'],
   ['Olst', 'dir_list',
       Args => '',
       Result_code => 'ROls',
       Result => 'count/int data/string'],
   ['Ocre', 'dir_create_file',
       Args => 'flags/int mode/int leaf/string',
       Result_code => 'ROcr',
       Result => 'fd'],
   ['Omkd', 'dir_mkdir',
       Args => 'mode/int leaf/string',
       Result => ''],
   ['Osym', 'dir_symlink',
       Args => 'leaf/len_string dest_path/string',
       Result => ''],
   ['Ornm', 'dir_rename',
       Args => 'leaf/len_string dest_dir/obj dest_leaf/string',
       Result => ''],
   ['Olnk', 'dir_link',
       Args => 'leaf/len_string dest_dir/obj dest_leaf/string',
       Result => ''],
   ['Ounl', 'dir_unlink',
       Args => 'leaf/string',
       Result => ''],
   ['Ormd', 'dir_rmdir',
       Args => 'leaf/string',
       Result => ''],
   ['Obnd', 'dir_socket_bind',
       Args => 'leaf/string sock/fd',
       Result => ''],
   # Symlink objects:
   ['Ordl', 'symlink_readlink',
       Args => '',
       Result_code => 'ROrl',
       Result => 'dest/string'],

   ['Mkco', 'make_conn'],
     ['RMkc', 'r_make_conn'],
   ['Mkc2', 'make_conn2'],
     ['RMc2', 'r_make_conn2'],
   ['Mkfs', 'make_fs_op',
       Args => 'root_dir/obj',
       Result_code => 'RMfs',
       Result => 'fs_op/obj'],
   ['Mkud', 'make_union_dir',
       Args => 'dir1/obj dir2/obj',
       Result_code => 'RMud',
       Result => 'dir/obj'],

   # Executable objects
   ['Exep', 'eo_is_executable'],
   ['Exeo', 'eo_exec',
       Result_code => 'REex'],

   # Powerbox
   ['Pbrf', 'powerbox_req_filename'],
   ['Pbgf', 'powerbox_result_filename'],
   ['Pbgc', 'powerbox_result_cancel'],

   # Used by shell
   ['Sopt', 'set_option'],
   ['Gopt', 'get_option'],

   ['Rsdr', 'resolve_dir'], # 'cdiS'
   ['Rsob', 'resolve_obj_simple'],
   ['Fsmn', 'fs_make_node'], # ''
   ['Fsap', 'fs_attach_at_path'], # 'cSc'
   ['Fsrp', 'fs_resolve_populate'],
   ['Fsdn', 'fs_dir_of_node'], # 'c'
   ['Fspt', 'fs_print_tree'],
   ['Dsrt', 'dirstack_root'],
   ['Dscs', 'dirstack_cons'],
   ['Dsdi', 'dirstack_get_dir'],
   ['Dspa', 'dirstack_get_parent'],
   ['Dspt', 'dirstack_get_path'],
   ['RCap', 'r_cap'], # 'c'
   ['RMsg', 'fail_unknown_method'],
  ];


# List of hashes:
#   Code
#   Name
my @message_ids;

my @py_decls = ('from plash_marshal import method_id', '');

my @defines;
my @funs;
my @ex;
my @ex_call;

foreach my $m (@$methods) {
  my ($id, $name, %x) = @$m;

  push(@message_ids,
       { Code => $id,
	 Name => $name });
  if(defined $x{Result_code}) {
    push(@message_ids,
	 { Code => $x{Result_code},
	   Name => "r_$name" });
  }

  push(@ex_call,
       indent('region_t r = region_make();',
	      'struct cap_args result;'));
  if(defined $x{Args}) {
    my $message_name = "METHOD_".uc($name);
    
    my $list = parse_arg_string($x{Args});
    my $a = make_unpacker($name, $message_name, $list);
    push(@funs,
	 @{$a->{Unpack_fun}},
	 @{$a->{Pack_fun}});
    push(@ex, indent(@{$a->{Unpack_ex}}));
    push(@ex_call, concat(['cap_call(obj, r, '], $a->{Pack_ex}, [', &result);']));
  }
  if(defined $x{Result}) {
    my $result_name;
    if(defined $x{Result_code}) {
      $result_name = "METHOD_R_".uc($name);
    }
    else {
      $result_name = "METHOD_OKAY";
      if($x{Result} ne '') { die "Need message code for non-empty results ($name)" }
    }
    
    my $a = make_unpacker($name.'_result', $result_name,
			  parse_arg_string($x{Result}));
    push(@funs,
	 @{$a->{Unpack_fun}},
	 @{$a->{Pack_fun}});
    push(@ex,
	 indent(concat(['*result = '], $a->{Pack_ex}, [';']),
		'return;'));
    push(@ex_call, @{$a->{Unpack_ex}});
  }
  push(@ex, '');
  push(@ex_call,
       indent('*err = ENOSYS;',
	      'rc = ..'),
       ' done:',
       indent('region_free(r);',
	      'return ..;'),
       '');
}

foreach my $x (@message_ids) {
  my $id_name = "METHOD_".uc($x->{Name});
  push(@defines,
       sprintf("#define %s %s0x%8x /* \"%s\" */",
	       $id_name,
	       ' ' x (30-length($id_name)),
	       unpack('I', $x->{Code}),
	       $x->{Code}));
  push(@py_decls,
       sprintf("method_id('%s', '%s')",
	       $x->{Name}, $x->{Code}));
}

if(1) {
  my $file = 'gensrc/out-marshal.h';
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file'" }
  print $f "/* Auto-generated by make-marshal.pl */\n\n";
  foreach(@defines) { print $f "$_\n" }
  print $f "\n";
  foreach(@funs) { print $f "$_\n" }
  $f->close();
}

if(1) {
  my $file = 'gensrc/out-marshal-examples';
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file'" }
  print $f "/* Auto-generated by make-marshal.pl */\n\n";
  foreach(@ex) { print $f "$_\n" }
  foreach(@ex_call) { print $f "$_\n" }
  $f->close();
}

if(1) {
  my $file = 'gensrc/methods.py';
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file'" }
  print $f "# Auto-generated by make-marshal.pl\n\n";
  foreach(@py_decls) { print $f "$_\n" }
  $f->close();
}


sub make_unpacker {
  my ($fun_name, $id_name, $args) = @_;

  my $no_cap_args = 0;

  my @f_args; # Arguments to function
  my @call_args; # Declaration of argument to pass to function as example
  my @data_args = ("m_int_const(&ok, &data, $id_name);"); # Code to match data
  my @cap_args; # Code to unpack caps
  my @fd_args; # Code to unpack FDs

  my @cons_args;
  my @data_out = ("mk_int(r, $id_name)");
  my @caps_out;
  my @fds_out;

  my @do_last;
  my $last = sub { push(@do_last, @_) };

  my %end;
  foreach my $arg (@$args) {
    my ($name, $ty) = @$arg;
    my $cl;
    my $final;
    if($ty eq 'int') {
      $cl = Data;
      $final = 0;
      push(@f_args, "int *arg_$name");
      push(@data_args, "m_int(&ok, &data, arg_$name);");
      
      push(@call_args, "int $name");

      push(@cons_args, "int arg_$name");
      push(@data_out, "mk_int(r, arg_$name)");
    }
    elsif($ty eq 'string') {
      $cl = Data;
      $final = 1;
      push(@f_args, "seqf_t *arg_$name");
      &$last(sub { push(@data_args, "*arg_$name = data;"); });
      
      push(@call_args, "seqf_t $name");
      
      push(@cons_args, "seqt_t arg_$name");
      &$last(sub { push(@data_out, "arg_$name"); }); # "mk_leaf(r, $name)"
    }
    elsif($ty eq 'len_string') {
      $cl = Data;
      $final = 0;
      push(@f_args, "seqf_t *arg_$name");
      push(@data_args, "m_lenblock(&ok, &data, arg_$name);");
      
      push(@call_args, "seqf_t $name");

      push(@cons_args, "seqt_t arg_$name");
      push(@data_out,
	   "mk_int(r, arg_$name.size)",
	   "arg_$name"); # "mk_leaf(r, $name)"
    }
    elsif($ty eq 'obj') {
      $cl = Cap;
      $final = 0;
      push(@f_args, "cap_t *arg_$name");
      push(@cap_args, "*arg_$name = args.caps.caps[$no_cap_args];");
      $no_cap_args++;
      
      push(@call_args, "cap_t $name");
      
      push(@cons_args, "cap_t arg_$name");
      push(@caps_out, "arg_$name");
    }
#    elsif($ty eq 'obj_seq') {
#      $cl = Cap;
#      $final = 0;
#      push(@f_args, "cap_seq_t *arg_$name");
#      &$last(sub {
#	       push(@cap_args,
#		    "arg_$name->caps = args.caps.caps + $no_cap_args;",
#		    "arg_$name->size = args.caps.size - $no_cap_args;");
#	     });
#
#      push(@call_args, "cap_seq_t $name");
#      
#      push(@cons_args, "cap_seq_t arg_$name");
#      push(@caps_out, "arg_$name");
#      $cap_args_rest = "arg_$name");
#    }
    elsif($ty eq 'fd') {
      $cl = FD;
      $final = 0;
      my $i = scalar(@fd_args);
      push(@f_args, "int *arg_$name");
      push(@fd_args, "*arg_$name = args.fds.fds[$i];");

      push(@call_args, "int $name");

      push(@cons_args, "int arg_$name");
      push(@fds_out, "arg_$name");
    }
    else { die "Unknown type: $ty" }
    die if !defined $cl;
    if($final) {
      if($end{$cl}) { die "Multiple `rest' arguments for $cl" }
      $end{$cl} = 1;
    }
  }
  foreach my $f (@do_last) { &$f() }

  my $r = {};

  $r->{Unpack_fun} =
      [sprintf("static inline int unpack_%s(region_t r, struct cap_args args%s)",
	       $fun_name,
	       join('', map { ", $_" } @f_args)),
       '{',
       indent
       (sprintf("if(args.caps.size == %i && args.fds.count == %i) {",
		$no_cap_args,
		scalar(@fd_args)),
	indent
	("seqf_t data = flatten_reuse(r, args.data);",
	 "int ok = 1;",
	 @data_args,
	 ($end{Data} ? () : ('m_end(&ok, &data);')),
	 "if(ok) {",
	 indent
	 (@cap_args, @fd_args, "return 0;"),
	 '}'),
	'}',
	'return -1;'),
       '}'];

  # Generate unpacker example code
  $r->{Unpack_ex} =
    ["case $id_name:", '{',
     indent((map { "$_;" } @call_args),
	    sprintf("if(unpack_%s(args%s) < 0) goto bad_msg;",
		    $fun_name,
		    join('', map { ', &'.$_->[0] } @$args))),
     '}'];
  
  # Generate packer
  my @main;
  if(scalar(@data_out) == 1) { @main = $data_out[0] }
  elsif(scalar(@data_out) == 0) { @main = 'seqf_empty' }
  else {
    @main =
      concat([sprintf('cat%i(r, ', scalar(@data_out))],
	     [split("\n", join(",\n", @data_out))],
	     [')'])
  }
  my $caps_expr;
  if(scalar(@caps_out) == 0) { $caps_expr = 'caps_empty' }
  else {
    $caps_expr = sprintf('mk_caps%i(r, %s)',
			 scalar(@caps_out),
			 join(', ', @caps_out));
  }
  my $fds_expr;
  if(scalar(@fds_out) == 0) { $fds_expr = 'fds_empty' }
  else {
    $fds_expr = sprintf('mk_fds%i(r, %s)',
			scalar(@fds_out),
			join(', ', @fds_out));
  }
  $r->{Pack_fun} =
    [sprintf('static inline struct cap_args pack_%s(%s)',
	     $fun_name,
	     join(', ', 'region_t r', @cons_args)),
     '{',
     indent(concat(['return cap_args_make('],
		   [concat([@main], [',']),
		    "$caps_expr,",
		    "$fds_expr"],
		   [');'])),
     '}'];

  $r->{Pack_ex} =
    [sprintf('pack_%s(%s)',
	     $fun_name,
	     join(', ', 'r', map { '..' } @$args))];

  $r
}

sub parse_arg_string {
  my ($arg_string) = @_;
  my @got;
  foreach my $a (split(/\s+/, $arg_string)) {
    if($a =~ m#^(([^/]+)/)?([^/]+)$#) {
      my $ty = $3;
      my $name = defined $2 ? $2 : $ty;
      push(@got, [$name, $ty]);
    }
    else { die "Bad argument: $a" }
  }
  \@got
}


sub indent {
  map { "  $_" } @_
}

sub concat {
  my @a;
  my $last = '';
  my $ind = 0;
  foreach my $list (@_) {
    my $first = 1;
    foreach my $line (@$list) {
      if($first) {
	$first = 0;
	$last .= $line;
      }
      else {
	push(@a, $last);
	$last = (' ' x $ind).$line;
      }
    }
    $ind = length($last);
  }
  (@a, $last)
}
