#!/usr/bin/perl -w

# Copyright (C) 2004 Mark Seaborn
#
# This file is part of Plash.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

use lib qw(./src);
use FileUtil qw(write_file_if_changed);

# Fields of a vtable.  All must be filled in.
my $methods =
  [qw(free
      mark
      cap_invoke
      cap_call
      single_use
      type
      stat
      utimes
      chmod
      open
      socket_connect
      traverse
      list
      create_file
      mkdir
      symlink
      rename
      link
      unlink
      rmdir
      socket_bind
      readlink
      slot_get
      slot_create_file
      slot_mkdir
      slot_symlink
      slot_unlink
      slot_rmdir
      slot_socket_bind
      make_conn
      make_conn2
      log_msg
      log_branch
      vtable_name)];
my $methods_hash = { map { $_ => 1 } @$methods };

# Default values for fields of the vtable.
my $defaults =
    { 'free' => undef,
      'mark' => undef,
      'cap_invoke' => 'local_obj_invoke',
      'cap_call' => 'marshal_cap_call',
      'single_use' => '0',
      'type' => 'objt_unknown',
      'stat' => 'dummy_stat',
      'utimes' => 'dummy_utimes',
      'chmod' => 'dummy_chmod',
      'open' => 'dummy_open',
      'socket_connect' => 'dummy_socket_connect',
      'traverse' => 'dummy_traverse',
      'list' => 'dummy_list',
      'create_file' => 'dummy_create_file',
      'mkdir' => 'dummy_mkdir',
      'symlink' => 'dummy_symlink',
      'rename' => 'dummy_rename_or_link',
      'link' => 'dummy_rename_or_link',
      'unlink' => 'dummy_unlink',
      'rmdir' => 'dummy_rmdir',
      'socket_bind' => 'dummy_socket_bind',
      'readlink' => 'dummy_readlink',
      'slot_get' => 'dummy_slot_get',
      'slot_create_file' => 'dummy_slot_create_file',
      'slot_mkdir' => 'dummy_slot_mkdir',
      'slot_symlink' => 'dummy_slot_symlink',
      'slot_unlink' => 'dummy_slot_unlink',
      'slot_rmdir' => 'dummy_slot_rmdir',
      'slot_socket_bind' => 'dummy_slot_socket_bind',
      'make_conn' => 'dummy_make_conn',
      'make_conn2' => 'dummy_make_conn2',
      'log_msg' => 'dummy_log_msg',
      'log_branch' => 'dummy_log_branch',
    };

my $bytes = 0;

# Interfaces:  these are just used to warn when fields haven't been filled out
my @i_fsobj = ('type', 'stat', 'utimes');
my @i_file = (@i_fsobj,
	      'chmod',
	      'open',
	      'socket_connect');
my @i_dir = (@i_fsobj,
	     'chmod',
	     'traverse',
	     'list',
	     'create_file',
	     'mkdir',
	     'symlink',
	     'rename',
	     'link',
	     'unlink',
	     'rmdir',
	     'socket_bind');
my @i_symlink = (@i_fsobj,
		 'readlink');
my @i_slot = ('slot_get',
	      'slot_create_file',
	      'slot_mkdir',
	      'slot_symlink',
	      'slot_unlink',
	      'slot_rmdir',
	      'slot_socket_bind');


# This is copied at run-time to generate one-off objects that only
# implement cap_call.
put('gensrc/out-vtable-defaults.h',
    [{ Name => 'defaults_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'generic_free'],
	  ['mark', 'NULL'],
	 ]
     }]);

put('gensrc/out-vtable-filesysobj.h',
    [{ Name => 'invalid_vtable',
       Interfaces => [@$methods],
       Contents =>
         [['free', 'generic_free'],
	  ['mark', 'invalid_mark'],
	  ['cap_invoke', 'invalid_cap_invoke'],
	  ['cap_call', 'invalid_cap_call'],
	  ['type', 'invalid_objt'],
	  ['stat', 'invalid_stat'],
	  ['utimes', 'invalid_utimes'],
	  ['chmod', 'invalid_chmod'],
	  ['open', 'invalid_open'],
	  ['socket_connect', 'invalid_socket_connect'],
	  ['traverse', 'invalid_traverse'],
	  ['list', 'invalid_list'],
	  ['create_file', 'invalid_create_file'],
	  ['mkdir', 'invalid_mkdir'],
	  ['symlink', 'invalid_symlink'],
	  ['rename', 'invalid_rename_or_link'],
	  ['link', 'invalid_rename_or_link'],
	  ['unlink', 'invalid_unlink'],
	  ['rmdir', 'invalid_rmdir'],
	  ['socket_bind', 'invalid_socket_bind'],
	  ['readlink', 'invalid_readlink'],
	  ['slot_get', 'NULL'],
	  ['slot_create_file', 'NULL'],
	  ['slot_mkdir', 'NULL'],
	  ['slot_symlink', 'NULL'],
	  ['slot_unlink', 'NULL'],
	  ['slot_rmdir', 'NULL'],
	  ['slot_socket_bind', 'NULL'],
	  ['make_conn', 'NULL'],
	  ['make_conn2', 'NULL'],
	  ['log_msg', 'invalid_log_msg'],
	  ['log_branch', 'invalid_log_branch'],
	 ]
     }]);


put('gensrc/out-vtable-filesysobj-real.h',
    [{ Name => 'real_file_vtable',
       Interfaces => [@i_file],
       Contents =>
         [['free', 'real_file_free'],
	  ['mark', 'NULL'],
	  ['type', 'objt_file'],
	  ['stat', 'real_file_stat'],
	  ['utimes', 'real_file_utimes'],
	  ['chmod', 'real_file_chmod'],
	  ['open', 'real_file_open'],
	  ['socket_connect', 'real_file_socket_connect'],
	 ]
     },
     { Name => 'real_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'real_dir_free'],
	  ['mark', 'NULL'],
	  ['type', 'objt_dir'],
	  ['stat', 'real_dir_stat'],
	  ['utimes', 'real_dir_utimes'],
	  ['chmod', 'real_dir_chmod'],
	  ['traverse', 'real_dir_traverse'],
	  ['list', 'real_dir_list'],
	  ['create_file', 'real_dir_create_file'],
	  ['mkdir', 'real_dir_mkdir'],
	  ['symlink', 'real_dir_symlink'],
	  ['rename', 'real_dir_rename'],
	  ['link', 'real_dir_link'],
	  ['unlink', 'real_dir_unlink'],
	  ['rmdir', 'real_dir_rmdir'],
	  ['socket_bind', 'real_dir_socket_bind'],
	 ]
     },
     { Name => 'real_symlink_vtable',
       Interfaces => [@i_symlink],
       Contents =>
         [['free', 'real_symlink_free'],
	  ['mark', 'NULL'],
	  ['type', 'objt_symlink'],
	  ['stat', 'real_symlink_stat'],
	  ['utimes', 'real_symlink_utimes'],
	  ['readlink', 'real_symlink_readlink'],
	 ]
     },
    ]);

put('gensrc/out-vtable-filesysobj-fab.h',
    [{ Name => 'fab_symlink_vtable',
       Interfaces => [@i_symlink],
       Contents =>
         [['free', 'fab_symlink_free'],
	  ['mark', 'NULL'],
	  ['type', 'objt_symlink'],
	  ['stat', 'fab_symlink_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['readlink', 'fab_symlink_readlink'],
	 ]
     },
     { Name => 'fab_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'fab_dir_free'],
	  ['mark', 'fab_dir_mark'],
	  ['type', 'objt_dir'],
	  ['stat', 'fab_dir_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['traverse', 'fab_dir_traverse'],
	  ['list', 'fab_dir_list'],
	  ['create_file', 'refuse_create_file'],
	  ['mkdir', 'refuse_mkdir'],
	  ['symlink', 'refuse_symlink'],
	  ['rename', 'refuse_rename_or_link'],
	  ['link', 'refuse_rename_or_link'],
	  ['unlink', 'refuse_unlink'],
	  ['rmdir', 'refuse_rmdir'],
	  ['socket_bind', 'refuse_socket_bind'],
	 ]
     },
     { Name => 's_fab_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 's_fab_dir_free'],
	  ['mark', 's_fab_dir_mark'],
	  ['type', 'objt_dir'],
	  ['stat', 's_fab_dir_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['traverse', 's_fab_dir_traverse'],
	  ['list', 's_fab_dir_list'],
	  ['create_file', 's_fab_dir_create_file'],
	  ['mkdir', 's_fab_dir_mkdir'],
	  ['symlink', 's_fab_dir_symlink'],
	  ['rename', 'refuse_rename_or_link'],
	  ['link', 'refuse_rename_or_link'],
	  ['unlink', 's_fab_dir_unlink'],
	  ['rmdir', 's_fab_dir_rmdir'],
	  ['socket_bind', 's_fab_dir_socket_bind'],
	 ]
     },
    ]);

put('gensrc/out-vtable-filesysslot.h',
    [{ Name => 'gen_slot_vtable',
       Interfaces => [@i_slot],
       Contents =>
         [['free', 'gen_slot_free'],
	  ['mark', 'gen_slot_mark'],
	  ['slot_get', 'gen_slot_get'],
	  ['slot_create_file', 'gen_slot_create_file'],
	  ['slot_mkdir', 'gen_slot_mkdir'],
	  ['slot_symlink', 'gen_slot_symlink'],
	  ['slot_unlink', 'gen_slot_unlink'],
	  ['slot_rmdir', 'gen_slot_rmdir'],
	  ['slot_socket_bind', 'gen_slot_socket_bind'],
	 ]
     },
     { Name => 'ro_slot_vtable',
       Interfaces => [@i_slot],
       Contents =>
         [['free', 'ro_slot_free'],
	  ['mark', 'ro_slot_mark'],
	  ['slot_get', 'ro_slot_get'],
	  ['slot_create_file', 'ro_slot_create_file'],
	  ['slot_mkdir', 'ro_slot_mkdir'],
	  ['slot_symlink', 'ro_slot_symlink'],
	  ['slot_unlink', 'ro_slot_unlink'],
	  ['slot_rmdir', 'ro_slot_rmdir'],
	  ['slot_socket_bind', 'ro_slot_socket_bind'],
	 ]
     },
    ]);

put('gensrc/out-vtable-filesysobj-readonly.h',
    [{ Name => 'readonly_obj_vtable',
       Interfaces => [@i_file, @i_dir, @i_symlink],
       Contents =>
         [['free', 'readonly_free'],
	  ['mark', 'readonly_mark'],
	  ['type', 'readonly_type'],
	  ['stat', 'readonly_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['open', 'readonly_open'],
	  ['socket_connect', 'refuse_socket_connect'],
	  ['traverse', 'readonly_traverse'],
	  ['list', 'readonly_list'],
	  ['create_file', 'refuse_create_file'],
	  ['mkdir', 'refuse_mkdir'],
	  ['symlink', 'refuse_symlink'],
	  ['rename', 'refuse_rename_or_link'],
	  ['link', 'refuse_rename_or_link'],
	  ['unlink', 'refuse_unlink'],
	  ['rmdir', 'refuse_rmdir'],
	  ['socket_bind', 'refuse_socket_bind'],
	  ['readlink', 'readonly_readlink']
	 ]
     }
    ]);

put('gensrc/out-vtable-filesysobj-union.h',
    [{ Name => 'union_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'union_dir_free'],
	  ['mark', 'union_dir_mark'],
	  ['type', 'objt_dir'],
	  ['stat', 'union_dir_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['traverse', 'union_dir_traverse'],
	  ['list', 'union_dir_list'],
	  ['create_file', 'refuse_create_file'],
	  ['mkdir', 'refuse_mkdir'],
	  ['symlink', 'refuse_symlink'],
	  ['rename', 'refuse_rename_or_link'],
	  ['link', 'refuse_rename_or_link'],
	  ['unlink', 'refuse_unlink'],
	  ['rmdir', 'refuse_rmdir'],
	  ['socket_bind', 'refuse_socket_bind'],
	 ]
     }
    ]);

put('gensrc/out-vtable-filesysobj-cow.h',
    [{ Name => 'cow_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'cow_dir_free'],
	  ['mark', 'cow_dir_mark'],
	  ['type', 'objt_dir'],
	  ['stat', 'cow_dir_stat'],
	  ['utimes', 'cow_dir_utimes'],
	  ['chmod', 'cow_dir_chmod'],
	  ['traverse', 'cow_dir_traverse'],
	  ['list', 'cow_dir_list'],
	  ['create_file', 'cow_dir_create_file'],
	  ['mkdir', 'cow_dir_mkdir'],
	  ['symlink', 'cow_dir_symlink'],
	  ['rename', 'cow_dir_rename'],
	  ['link', 'cow_dir_link'],
	  ['unlink', 'cow_dir_unlink'],
	  ['rmdir', 'cow_dir_rmdir'],
	  ['socket_bind', 'cow_dir_socket_bind'],
	 ]
     }
    ]);

my $marshal_methods =
  [
	  ['type', 'marshal_type'],
	  ['stat', 'marshal_stat'],
	  ['utimes', 'marshal_utimes'],
	  ['chmod', 'marshal_chmod'],
	  ['open', 'marshal_open'],
	  ['socket_connect', 'marshal_socket_connect'],
	  ['traverse', 'marshal_traverse'],
	  ['list', 'marshal_list'],
	  ['create_file', 'marshal_create_file'],
	  ['mkdir', 'marshal_mkdir'],
	  ['symlink', 'marshal_symlink'],
	  ['rename', 'marshal_rename'],
	  ['link', 'marshal_link'],
	  ['unlink', 'marshal_unlink'],
	  ['rmdir', 'marshal_rmdir'],
	  ['socket_bind', 'marshal_socket_bind'],
	  ['readlink', 'marshal_readlink'],
          ['make_conn', 'marshal_make_conn'],
  ];

put('gensrc/out-vtable-cap-protocol.h',
    [{ Name => 'remote_obj_vtable',
       Interfaces => [@$methods],
       Contents =>
         [['free', 'remote_obj_free'],
	  ['mark', 'NULL'],
	  ['cap_invoke', 'remote_obj_invoke'],
	  ['cap_call', 'generic_obj_call'],
	  ['single_use', '0'],
	  @$marshal_methods
	 ]
     }
    ]);

put('gensrc/out-vtable-cap-call-return.h',
    [{ Name => 'return_cont_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'return_cont_free'],
	  ['mark', 'NULL'],
	  ['cap_invoke', 'return_cont_invoke'],
	  ['cap_call', 'generic_obj_call'],
	  ['single_use', '1'],
	 ]
     }
    ]);

put('gensrc/out-vtable-fs-operations.h',
    [{ Name => 'fs_op_vtable',
       Contents =>
         [['free', 'fs_op_free'],
	  ['mark', 'fs_op_mark'],
	  ['cap_call', 'fs_op_call'],
	 ]
     },
     { Name => 'fs_op_maker_vtable',
       Contents =>
         [['free', 'fs_op_maker_free'],
	  ['mark', 'NULL'],
	  ['cap_call', 'fs_op_maker_call'],
	 ]
     },
     { Name => 'conn_maker_vtable',
       Contents =>
         [['free', 'conn_maker_free'],
	  ['mark', 'NULL'],
	  ['make_conn', 'conn_maker_make_conn'],
	 ]
     },
     { Name => 'union_dir_maker_vtable',
       Contents =>
         [['free', 'generic_free'],
	  ['mark', 'NULL'],
	  ['cap_call', 'union_dir_maker_call'],
	 ]
     },
    ]);

put('gensrc/out-vtable-log-proxy.h',
    [{ Name => 'log_proxy_vtable',
       Contents =>
         [['free', 'log_proxy_free'],
	  ['mark', 'log_proxy_mark'],
	  ['cap_call', 'log_proxy_call'],
	  @$marshal_methods
	 ]
     }
    ]);

put('gensrc/out-vtable-shell.h',
    [{ Name => 'd_conn_maker_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'd_conn_maker_free'],
	  ['mark', 'NULL'],
	  ['make_conn', 'd_make_conn'],
	  ['make_conn2', 'd_make_conn2'],
	 ]
     },
     { Name => 'proc_return_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'proc_return_free'],
	  ['mark', 'NULL'],
	  ['cap_invoke', 'proc_return_invoke'],
	 ]
     },
     { Name => 'options_obj_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'options_obj_free'],
	  ['mark', 'NULL'],
	  ['cap_call', 'options_obj_call'],
	 ]
     },
    ]);

put('gensrc/out-vtable-reconnectable-obj.h',
    [{ Name => 'reconnectable_obj_vtable',
       Interface => [@$methods],
       Contents =>
         [['free', 'reconnectable_obj_free'],
	  ['mark', 'reconnectable_obj_mark'],
	  ['cap_invoke', 'reconnectable_obj_invoke'],
	  ['cap_call', 'reconnectable_obj_call'],
	  ['single_use', '0'],
	  @$marshal_methods
	 ]
     }
    ]);

put('gensrc/out-vtable-resolve-filename.h',
    [{ Name => 'dir_stack_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'dir_stack_method_free'],
	  ['mark', 'dir_stack_method_mark'],
	 ]
     }
    ]);

put('gensrc/out-vtable-build-fs.h',
    [{ Name => 'node_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'node_free'],
	  ['mark', 'node_mark'],
	 ]
     }
    ]);
put('gensrc/out-vtable-build-fs-dynamic.h',
    [{ Name => 'comb_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'comb_dir_free'],
	  ['mark', 'comb_dir_mark'],
	  ['type', 'objt_dir'],
	  ['stat', 'comb_dir_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['traverse', 'comb_dir_traverse'],
	  ['list', 'comb_dir_list'],
	  ['create_file', 'comb_dir_create_file'],
	  ['mkdir', 'comb_dir_mkdir'],
	  ['symlink', 'comb_dir_symlink'],
	  ['rename', 'comb_dir_rename'],
	  ['link', 'comb_dir_link'],
	  ['unlink', 'comb_dir_unlink'],
	  ['rmdir', 'comb_dir_rmdir'],
	  ['socket_bind', 'comb_dir_socket_bind'],
	 ]
     }
    ]);

put('gensrc/out-vtable-log.h',
    [{ Name => 'log_stream_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'log_free'],
	  ['mark', 'log_mark'],
	  ['log_msg', 'log_msg'],
	  ['log_branch', 'log_branch'],
	 ]
     }
    ]);

put('gensrc/out-vtable-exec-object.h',
    [{ Name => 'exec_obj_vtable',
       Interfaces => [@i_file],
       Contents =>
         [['free', 'exec_obj_free'],
	  ['mark', 'exec_obj_mark'],
	  ['cap_invoke', 'exec_obj_invoke'],
	  ['cap_call', 'exec_obj_call'], # deals with `exec'
	  ['type', 'objt_file'],
	  ['stat', 'exec_obj_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['open', 'refuse_open'],
	  ['socket_connect', 'refuse_socket_connect'],
	 ]
     }
    ]);
put('gensrc/out-vtable-run-emacs.h',
    [{ Name => 'edit_obj_vtable',
       Interfaces => [@i_file],
       Contents =>
         [['free', 'edit_obj_free'],
	  ['mark', 'edit_obj_mark'],
	  ['cap_invoke', 'edit_obj_invoke'],
	  ['cap_call', 'edit_obj_call'], # deals with `exec'
	  ['type', 'objt_file'],
	  ['stat', 'edit_obj_stat'],
	  ['utimes', 'refuse_utimes'],
	  ['chmod', 'refuse_chmod'],
	  ['open', 'refuse_open'],
	  ['socket_connect', 'refuse_socket_connect'],
	 ]
     }
    ]);

put('gensrc/out-vtable-powerbox.h',
    [{ Name => 'powerbox_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'powerbox_free'],
	  ['mark', 'powerbox_mark'],
	  ['cap_invoke', 'powerbox_invoke'],
	 ]
     }
    ]);

put('gensrc/out-vtable-powerbox-for-gtk.h',
    [{ Name => 'pb_return_cont_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'pb_return_cont_free'],
          ['mark', 'pb_return_cont_mark'],
	  ['cap_invoke', 'pb_return_cont_invoke']
	 ]
     }
    ]);

put('gensrc/out-vtable-python.h',
    [{ Name => 'plpy_pyobj_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'plpy_pyobj_free'],
	  ['mark', 'NULL'],
	  ['cap_invoke', 'plpy_pyobj_cap_invoke'],
	  ['cap_call', 'plpy_pyobj_cap_call'],
	  @$marshal_methods
	 ]
     }]);

print "vtables use $bytes bytes\n";


sub put {
  my ($file, $list) = @_;

  print "Writing $file\n";

  my @out;
  push(@out, "/* Auto-generated by make-vtables.pl */");

  # for local_obj_invoke
  push(@out, "#include \"cap-protocol.h\"");
  
  foreach my $vtable (@$list) {
    put_vtable(\@out, $vtable);
  }

  my $data = join('', map { "$_\n" } @out);
  write_file_if_changed($file, $data);
}

sub put_vtable {
  my ($out, $vtable) = @_;

  my %got;
  foreach my $x (@{$vtable->{Contents}}) {
    if(!defined $methods_hash->{$x->[0]}) {
      die "Unknown method: $x->[0]"
    }
    if(defined $got{$x->[0]}) {
      die "Method defined twice: $x->[0]"
    }
    $got{$x->[0]} = $x->[1];
  }

  if(defined $vtable->{Interfaces}) {
    foreach my $meth (@{$vtable->{Interfaces}}) {
      if(!defined $got{$meth}) {
	print "$vtable->{Name} is missing `$meth' (using default)\n";
      }
    }
  }

  push(@$out, '');
  push(@$out, "const struct filesys_obj_vtable $vtable->{Name} = {");
  foreach my $meth (@$methods) {
    my $x = $got{$meth};
    if(!defined $x) {
      if($meth eq 'vtable_name') {
	$x = "\"$vtable->{Name}\""
      }
      elsif(defined $defaults->{$meth}) {
	$x = $defaults->{$meth}
      }
      else { die "$vtable->{Name}: No default for `$meth'" }
    }
    push(@$out, "#ifdef GC_DEBUG") if $meth eq 'mark';
    push(@$out, sprintf("  /* .%s = */ %s,",
			$meth, $x));
    push(@$out, "#endif") if $meth eq 'mark';
  }
  push(@$out, "  1 /* sentinel for type-checking */");
  push(@$out, "};");

  $bytes += scalar(@$methods) * 4;
}
