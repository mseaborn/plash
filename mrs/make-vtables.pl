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

# Fields of a vtable.  All must be filled in.
my $methods =
  [qw(free
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
      make_fs_op
      make_union_dir
      vtable_name)];
my $methods_hash = { map { $_ => 1 } @$methods };

# Default values for fields of the vtable.
my $defaults =
    { 'free' => undef,
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
      'make_fs_op' => 'dummy_make_fs_op',
      'make_union_dir' => 'dummy_make_union_dir',
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


put('mrs/out-vtable-filesysobj-real.h',
    [{ Name => 'real_file_vtable',
       Interfaces => [@i_file],
       Contents =>
         [['free', 'real_file_free'],
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
	  ['type', 'objt_symlink'],
	  ['stat', 'real_symlink_stat'],
	  ['utimes', 'real_symlink_utimes'],
	  ['readlink', 'real_symlink_readlink'],
	 ]
     },
    ]);

put('mrs/out-vtable-filesysobj-fab.h',
    [{ Name => 'fab_symlink_vtable',
       Interfaces => [@i_symlink],
       Contents =>
         [['free', 'fab_symlink_free'],
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

put('mrs/out-vtable-filesysslot.h',
    [{ Name => 'gen_slot_vtable',
       Interfaces => [@i_slot],
       Contents =>
         [['free', 'gen_slot_free'],
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

put('mrs/out-vtable-filesysobj-readonly.h',
    [{ Name => 'readonly_obj_vtable',
       Interfaces => [@i_file, @i_dir, @i_symlink],
       Contents =>
         [['free', 'readonly_free'],
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

put('mrs/out-vtable-filesysobj-union.h',
    [{ Name => 'union_dir_vtable',
       Interfaces => [@i_dir],
       Contents =>
         [['free', 'union_dir_free'],
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
          map { [$_, "marshal_$_"] } qw(make_conn make_fs_op make_union_dir)
  ];

put('mrs/out-vtable-cap-protocol.h',
    [{ Name => 'remote_obj_vtable',
       Interfaces => [@$methods],
       Contents =>
         [['free', 'remote_obj_free'],
	  ['cap_invoke', 'remote_obj_invoke'],
	  ['cap_call', 'generic_obj_call'],
	  ['single_use', '0'],
	  @$marshal_methods
	 ]
     }
    ]);

put('mrs/out-vtable-cap-call-return.h',
    [{ Name => 'return_cont_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'return_cont_free'],
	  ['cap_invoke', 'return_cont_invoke'],
	  ['cap_call', 'generic_obj_call'],
	  ['single_use', '1'],
	 ]
     }
    ]);

put('mrs/out-vtable-fs-operations.h',
    [{ Name => 'conn_maker_vtable',
       Contents =>
         [['free', 'conn_maker_free'],
	  ['make_conn', 'conn_maker_make_conn'],
	 ]
     }
    ]);

put('mrs/out-vtable-log-proxy.h',
    [{ Name => 'log_proxy_vtable',
       Contents =>
         [['free', 'log_proxy_free'],
	  ['cap_call', 'log_proxy_call'],
	  @$marshal_methods
	 ]
     }
    ]);

put('mrs/out-vtable-shell.h',
    [{ Name => 'd_conn_maker_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'd_conn_maker_free'],
	  ['make_conn', 'd_make_conn'],
	  ['make_conn2', 'd_make_conn2'],
	 ]
     },
     { Name => 'proc_return_vtable',
       Interfaces => [],
       Contents =>
         [['free', 'proc_return_free'],
	  ['cap_invoke', 'proc_return_invoke'],
	 ]
     },
    ]);

put('mrs/out-vtable-reconnectable-obj.h',
    [{ Name => 'reconnectable_obj_vtable',
       Interface => [@$methods],
       Contents =>
         [['free', 'reconnectable_obj_free'],
	  ['cap_invoke', 'reconnectable_obj_invoke'],
	  ['cap_call', 'reconnectable_obj_call'],
	  ['single_use', '0'],
	  @$marshal_methods
	 ]
     }
    ]);

put('mrs/out-vtable-exec-object.h',
    [{ Name => 'exec_obj_vtable',
       Interfaces => [@i_file],
       Contents =>
         [['free', 'exec_obj_free'],
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

print "vtables use $bytes bytes\n";


sub put {
  my ($file, $list) = @_;

  print "Writing $file\n";
  my $f = IO::File->new($file, 'w');
  if(!defined $f) { die "Can't open `$file'" }
  print $f "/* Auto-generated by make-vtables.pl */\n";

  # for local_obj_invoke
  print $f "#include \"cap-protocol.h\"\n";
  
  foreach my $vtable (@$list) {
    put_vtable($f, $vtable);
  }
  $f->close();
}

sub put_vtable {
  my ($f, $vtable) = @_;

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

  print $f "\n";
  print $f "struct filesys_obj_vtable $vtable->{Name} = {\n";
  foreach my $meth (@$methods) {
    my $x = $got{$meth};
    if(!defined $x) {
      if($meth eq 'vtable_name') {
	$x = "\"$vtable->{Name}\""
      }
      elsif(defined $defaults->{$meth}) {
	$x = $defaults->{$meth}
      }
      else { die "No default for `$meth'" }
    }
    printf $f ("  /* .%s = */ %s,\n",
	       $meth, $x);
  }
  print $f "  1 /* sentinel for type-checking */\n";
  print $f "};\n";

  $bytes += scalar(@$methods) * 4;
}
