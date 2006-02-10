(* Copyright (C) 2004 Mark Seaborn

   This file is part of Parser Generator.

   Parser Generator is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   Parser Generator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Parser Generator; see the file COPYING.  If not, write
   to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA.  *)


open StdLabels


type 'a result = OK of int * 'a | FAIL

let rightmost_err_pos = ref 0
let rightmost_err_msgs = (ref [] : string list ref)
let add_err i msg =
  if i = !rightmost_err_pos then
    rightmost_err_msgs := msg::!rightmost_err_msgs
  else if i > !rightmost_err_pos then
    (rightmost_err_pos := i;
     rightmost_err_msgs := [msg])

let do_parse f str =
  f str 0 (String.length str)

let output_errors ch =
  Printf.fprintf stderr "at %i:\n" !rightmost_err_pos;
  List.iter !rightmost_err_msgs
    ~f:(fun msg -> Printf.fprintf stderr "  expected %s\n" msg)
