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


let rec generate fmt = function
  | P_fail -> Format.fprintf fmt "FAIL"
  | P_null -> Format.fprintf fmt "OK (i, ())"
  | P_end ->
      Format.fprintf fmt "if i < j then (add_err i \"end\"; FAIL) else OK (i, ())"
  | P_seq(list, map_expr) ->
      let rec f fmt = function
        | (exp, bind_var)::rest ->
            Format.fprintf fmt
              "(@[<1>match %a with@ | FAIL -> FAIL@ | @[<1>OK (i, %s) ->@ %a@]@])"
              generate exp
              (match bind_var with
		 | Some var -> var
                 | None -> "_")
              f rest
        | [] -> Format.fprintf fmt "OK (i, (%s))"
                  (match map_expr with Some e -> e | None -> "()")
      in f fmt list
  | P_choice(exp1, exp2) ->
      Format.fprintf fmt
        "(@[<1>match %a with@ | OK (i, x) -> OK (i, x)@ | @[<1>FAIL ->@ %a@]@])"
        generate exp1 generate exp2
  | P_char(c) ->
      Format.fprintf fmt
        "(if i < j && str.[i] = '%s' then OK (i+1, ()) else (add_err i \"'%s'\"; FAIL))"
        (Char.escaped c) (String.escaped (Char.escaped c))
  | P_char_any ->
      Format.fprintf fmt
        "(if i < j then OK (i+1, str.[i]) else (add_err i \"any char\"; FAIL))"
  | P_char_class(list) ->
      Format.fprintf fmt
        "(@[if i < j && (let c = str.[i] in %s)@ then OK (i+1, ())@ else (add_err i \"char class\"; FAIL@]))"
        (String.concat ~sep:" || "
          (List.map list
            ~f:(fun (c1, c2) ->
		 if c1 = c2
		 then Printf.sprintf "c = '%s'" (Char.escaped c1)
		 else Printf.sprintf "'%s' <= c && c <= '%s'"
		        (Char.escaped c1) (Char.escaped c2))))
  | P_string(s) ->
      let rec f k =
        if k < String.length s then
          (Printf.sprintf "str.[i+%i] = '%s'" k (Char.escaped s.[k]))::(f (k+1))
        else []
      in
      Format.fprintf fmt
        "(@[if i+%i <= j && %s@ \
            then OK (i+%i, \"%s\")@ \
            else (add_err i \"\\\"%s\\\"\"; FAIL)@])"
        (String.length s)
        (String.concat ~sep:" && " (f 0))
        (String.length s)
        (String.escaped s)
        (String.escaped (String.escaped s))
  | P_var(var) ->
      Format.fprintf fmt "f_%s str i j" var
  | P_not(exp) ->
      Format.fprintf fmt
        "(@[<1>match %a with@ | OK (_, _) -> FAIL@ | FAIL -> OK (i, ())@])"
        generate exp
  | P_get_string(exp) ->
      Format.fprintf fmt
        "(@[<1>match %a with@ \
            | OK (i2, _) -> OK (i2, String.sub str ~pos:i ~len:(i2-i))@ \
            | FAIL -> FAIL@])"
        generate exp
  | P_opt(_) | P_repeat(_) | P_repeatplus(_) ->
      failwith "opt, repeat and repeatplus need to be expanded out"



let generate_defs fmt list =
  let rec f first = function
    | [] -> ()
    | (var, exp)::rest ->
        (if first then
           Format.fprintf fmt "@[<2>let rec"
	 else
	   Format.fprintf fmt "@ @[<2>and");
        (if false then
           Format.fprintf fmt
	     " f_%s str i j =@ let x = %a in@ (match x with OK (i2, _) -> Printf.printf \"matched %s from %%i to %%i\\n\" i i2 | FAIL -> Printf.printf \"didn't match %s at %%i\\n\" i);@ x@]"
             var generate exp var var
         else
           Format.fprintf fmt " f_%s str i j =@ %a@]" var generate exp);
        f false rest
  in
    Format.fprintf fmt "@[";
    f true list;
    Format.fprintf fmt "@]"


let output ch defs =
  output_string ch "\nopen Parser_runtime\n\n";
  let fmt = Format.formatter_of_out_channel ch in
    generate_defs fmt defs;
    Format.pp_print_flush fmt ();
    output_char ch '\n';
    flush ch
