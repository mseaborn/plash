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


type p_expr =
  | P_fail
  | P_null
  | P_end (* matches the end of the string only *)
  (* The second arg is a mapping expression, ie. semantic action.
     Omitting it gives `()' (unit) in OCaml or a null value in C. *)
  | P_seq of (p_expr * string option) list * string option
  | P_choice of p_expr * p_expr
  | P_char of char
  | P_char_any
  | P_char_class of (char * char) list
  | P_var of string (* name of non-terminal *)
  | P_string of string
  | P_not of p_expr
  | P_repeat of p_expr
  | P_repeatplus of p_expr
  | P_opt of p_expr
  | P_get_string of p_expr (* returns as a value the substring consumed *)
