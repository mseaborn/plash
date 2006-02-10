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


let rec reduce_right ~f ~id = function
  | [] -> id (* Only reached if the list was empty to start with *)
  | [x] -> x
  | x::xs -> f x (reduce_right ~f ~id xs)

let string_of_list list =
  let str = String.create (List.length list) in
    List.fold_left list ~init:0 ~f:(fun i c -> str.[i] <- c; i+1);
    str

let input_all_string ch =
  let rec f() =
    let buf = String.make 256 '\000' in
    let got = input ch buf 0 (String.length buf) in
      if got = String.length buf then buf::(f()) else
      if got = 0 then [] else
        (String.sub buf ~pos:0 ~len:got)::(f())
  in String.concat ~sep:"" (f())

let with_input file ~f =
  let ch = open_in file in
    match (try `OK (f ch)
	   with e -> `Exn e) with
      | `OK result -> close_in ch; result
      | `Exn e -> close_in ch; raise e
