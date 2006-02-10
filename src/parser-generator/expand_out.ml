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
open Pexprs


let add_def defs var exp = defs := (var, exp)::!defs

let fresh_name =
  let c = ref 0 in
    (fun()-> let i = !c in c := i+1; "tmp"^(string_of_int i))


let p_seq(list, map_expr) = P_seq(list, Some map_expr)

let rec simplify ~defs = function
  | (P_fail | P_null | P_end | P_var(_) |
     P_char(_) | P_char_any | P_char_class(_) | P_string(_)) as exp -> exp
  | P_seq(list, mexp) ->
      P_seq(List.map list
              ~f:(fun (exp, binding) -> (simplify ~defs exp, binding)),
            mexp)
  | P_choice(exp1, exp2) -> P_choice(simplify ~defs exp1, simplify ~defs exp2)
  | P_not(exp) -> P_not(simplify ~defs exp)
  | P_get_string(exp) -> P_get_string(simplify ~defs exp)
  | P_repeat(exp) ->
      let var = fresh_name() in
        add_def defs var
          (P_choice(p_seq([(simplify ~defs exp, Some "x");
                           (P_var var, Some "xs")],
                          "x::xs"),
                    p_seq([], "[]")));
        P_var var
  | P_repeatplus(exp) ->
      let var = fresh_name() in
        add_def defs var
          (p_seq([(simplify ~defs exp, Some "x");
                  (P_choice (P_var var,
                             p_seq([], "[]")),
                   Some "xs")],
                 "x::xs"));
        P_var var
  | P_opt(exp) ->
      P_choice(p_seq([(simplify ~defs exp, Some "x")],
                     "Some x"),
               p_seq([], "None"))


(* C version.  Does not generate any interesting semantic values from
   repeat etc. *)

module C = struct
let rec simplify ~defs = function
  | (P_fail | P_null | P_end | P_var(_) |
     P_char(_) | P_char_any | P_char_class(_) | P_string(_)) as exp -> exp
  | P_seq(list, mexp) ->
      P_seq(List.map list
              ~f:(fun (exp, binding) -> (simplify ~defs exp, binding)),
            mexp)
  | P_choice(exp1, exp2) -> P_choice(simplify ~defs exp1, simplify ~defs exp2)
  | P_not(exp) -> P_not(simplify ~defs exp)
  | P_get_string(exp) -> P_get_string(simplify ~defs exp)
  | P_repeat(exp) ->
      let var = fresh_name() in
        add_def defs var
          (P_choice(P_seq([(simplify ~defs exp, Some "x");
                           (P_var var, Some "xs")],
                          None),
                    P_seq([], None)));
        P_var var
  | P_repeatplus(exp) ->
      let var = fresh_name() in
        add_def defs var
          (P_seq([(simplify ~defs exp, Some "x");
                  (P_choice (P_var var,
                             P_seq([], None)),
                   Some "xs")],
                 None));
        P_var var
  | P_opt(exp) ->
      P_choice(P_seq([(simplify ~defs exp, Some "x")],
                     None),
               P_seq([], None))
end
