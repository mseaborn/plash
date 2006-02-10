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

(* PEG function:
   int parse(char *pos, char *end, char **pos_out, void **val_out)
*)

(*

'c'
=>
if(pos < end && *pos == 'c') {
  pos++;
  goto ok;
}
else goto fail;

EXP1 EXP2
=>
EXP1(label, fail)
label:
EXP2(ok, fail)

EXP1 / EXP2
=>
EXP1(ok, label)
label:
EXP2(ok, fail)

*)


(* Variables are declared with T_var: these are hoisted to the top of
   the C function. *)
type tree =
  | T_var of string
  | T_stmt of string
  | T_label of string
  | T_list of tree list


let fresh_name =
  let c = ref 0 in
    (fun prefix -> let i = !c in c := i+1; prefix^(string_of_int i))


(* char *pos_in:  current position in string being parsed
   char *end:     end of string
   On a successful parse:
     char *pos_out  is set to the new (consumed-upto) position in string
     void *ok_val   is set to the result value of the parse
     then goto ok
   On a failure, goto fail
*)

(* Consider:
   (e:exp ... {{ E1 }}) / (e:exp ... {{ E2 }})
   The generated code maps the variable "e" to "resultM" and "resultN"
   in the two different contexts, rather than using the name "e".
   That's because all the variables are lifted to the top of the
   function.  If we used the name "e", it would appear twice, which
   is an error.
   Using the temporary variables means we have to rebind them to
   their user-provided names in limited contexts in which E1 and E2
   appear, ie. "{ void *e = resultM; ...; resultK = E1; }".
*)

type extra_arg = { decl : string; name : string }
type options = {
  extra_args : extra_arg list;
  (* inhibit_actions:  If true, don't include value-constructing action
     code; ie. just produce a recogniser *)
  inhibit_actions : bool;
  track_fail_pos : bool;
}

let add_err ~opts ~pos =
  if opts.track_fail_pos then
    T_stmt (Printf.sprintf "if(*err_pos < %s) *err_pos = %s;" pos pos)
  else T_list []

let rec generate ~opts ~env ~pos_in ~pos_out ~ok_val ~ok ~fail = function
  | P_fail ->
      T_stmt (Printf.sprintf "goto %s;" fail)
  | P_null ->
      T_list
       [T_stmt (Printf.sprintf "%s = %s;" pos_out pos_in);
        T_stmt (Printf.sprintf "%s = 0;" ok_val);
        T_stmt (Printf.sprintf "goto %s;" ok)]
  | P_end ->
     T_list
      [add_err ~opts ~pos:pos_in;
       T_stmt (Printf.sprintf
                 "if(%s < end) goto fail; else { %s = %s; %s = 0; goto %s; }"
                 pos_in pos_out pos_in ok_val ok)]
  | P_seq(list, map_expr) ->
      let rec f ~env ~pos_in = function
        | (exp, bind_var)::rest ->
            let pos = fresh_name "pos" in
            let label = fresh_name "label" in
            let result = fresh_name "result" in
	      T_list
	      [T_var (Printf.sprintf "const char *%s" pos);
               T_var (Printf.sprintf "void *%s" result);
               generate ~opts ~env ~pos_in ~pos_out:pos ~ok_val:result ~ok:label ~fail exp;
	       T_label label;
	       f ~env:(match bind_var with
                         | Some v -> (v, result)::env
                         | None -> env)
                 ~pos_in:pos rest]
        | [] ->
            T_list
            [T_stmt (Printf.sprintf "%s = %s;" pos_out pos_in);
             T_stmt
               (let rebind =
		  String.concat ~sep:""
	            (List.map env
		       ~f:(fun (v, v2) ->
			     Printf.sprintf "void *%s = %s; " v v2)) in
                let map_expr' = match map_expr with Some e -> e | None -> "0" in
		if opts.inhibit_actions
		then Printf.sprintf "{ %s/* %s = %s; */ }"
		       rebind ok_val map_expr'
		else Printf.sprintf "{ %s%s = %s; }"
		       rebind ok_val map_expr');
             T_stmt (Printf.sprintf "goto %s;" ok)]
      in f ~env ~pos_in list
  | P_choice(exp1, exp2) ->
      let label = fresh_name "label" in
      T_list
       [generate ~opts ~env ~pos_in ~pos_out ~ok_val ~ok ~fail:label exp1;
        T_label label;
	generate ~opts ~env ~pos_in ~pos_out ~ok_val ~ok ~fail exp2]
  | P_char(c) ->
     T_list
      [add_err ~opts ~pos:pos_in;
       T_stmt
        (Printf.sprintf
          "if(%s < end && *%s == '%s') { %s = %s + 1; %s = 0; goto %s; } \
           else goto %s;"
          pos_in pos_in (Char.escaped c)
          pos_out pos_in ok_val ok
          fail)]
  | P_char_any ->
     T_list
      [add_err ~opts ~pos:pos_in;
       T_stmt
        (Printf.sprintf
          "if(%s < end) { %s = %s + 1; %s = (void*) (int) *%s; goto %s; } else goto %s;"
          pos_in
          pos_out pos_in ok_val pos_in ok
          fail)]
  | P_char_class(list) ->
     T_list
      [add_err ~opts ~pos:pos_in;
       T_stmt
        (Printf.sprintf
          "if(%s < end && (%s)) { %s = %s + 1; %s = (void*) (int) *%s; goto %s; } else goto %s;"
          pos_in
          (String.concat ~sep:" || "
            (List.map list
              ~f:(fun (c1, c2) ->
                    if c1 = c2
                    then Printf.sprintf "*%s == '%s'" pos_in (Char.escaped c1)
                    else Printf.sprintf "('%s' <= *%s && *%s <= '%s')"
                           (Char.escaped c1) pos_in pos_in (Char.escaped c2))))
          pos_out pos_in ok_val pos_in ok
          fail)]
  | P_string(s) ->
      let rec f k =
        if k < String.length s then
	  (Printf.sprintf "%s[%i] == '%s'" pos_in k (Char.escaped s.[k]))::
	  (f (k+1))
	else []
      in
      T_list
       [add_err ~opts ~pos:pos_in;
        T_stmt
        (Printf.sprintf
           "if(%s + %i <= end && %s) { %s = %s + %i; %s = 0; goto %s; } else goto %s;"
	   pos_in (String.length s)
	   (String.concat ~sep:" && " (f 0))
	   pos_out pos_in (String.length s) ok_val ok
	   fail)]
  | P_var(var) ->
      T_stmt
      (Printf.sprintf
         "{ const char *pos_out; void *ok_val; \
            if(f_%s(%s%s, end, &pos_out, &ok_val)) \
              { %s = pos_out; %s = ok_val; goto %s; } \
              else goto %s; \
          }"
         var
	 (String.concat ~sep:""
	    (List.map opts.extra_args ~f:(fun a -> a.name^", ")))
	 pos_in
         pos_out ok_val ok
         fail)
  | P_not(exp) ->
      let junk1 = fresh_name "junk_pos" in
      let junk2 = fresh_name "junk_pos" in
      let label = fresh_name "label" in
      T_list
       [T_var (Printf.sprintf "const char *%s" junk1);
        T_var (Printf.sprintf "const char *%s" junk2);
        generate ~opts ~env ~pos_in ~pos_out:junk1 ~ok_val:junk2 ~ok:fail ~fail:label exp;
	T_label label;
	T_stmt (Printf.sprintf "%s = %s; %s = 0; goto %s;"
			       pos_out pos_in ok_val ok)]
  | P_get_string(exp) ->
      failwith "get_string not implemented";
      (* FIXME *)
      generate ~opts ~env ~pos_in ~pos_out ~ok_val ~ok ~fail exp
  | P_opt(_) | P_repeat(_) | P_repeatplus(_) ->
      failwith "opt, repeat and repeatplus need to be expanded out"



let rec print_var_decls ch = function
  | T_list list -> List.iter ~f:(fun x -> print_var_decls ch x) list
  | T_stmt _ -> ()
  | T_label _ -> ()
  | T_var x -> Printf.fprintf ch "  %s;\n" x

let rec print_body ch = function
  | T_list list -> List.iter ~f:(fun x -> print_body ch x) list
  | T_stmt x -> Printf.fprintf ch "  %s\n" x
  | T_label x -> Printf.fprintf ch " %s:\n" x
  | T_var _ -> ()

let generate_defs ~opts ch list =
  Printf.fprintf ch "\n";
  let fun_decl var =
    Printf.fprintf ch
      "int f_%s(%sconst char *pos_in1, const char *end, \
                const char **pos_out_p, void **ok_val_p)"
      var (String.concat ~sep:""
             (List.map opts.extra_args ~f:(fun a -> a.decl^", ")));
  in
  List.iter list
    ~f:(fun (var, exp) ->
	  fun_decl var;
	  Printf.fprintf ch ";\n");
  List.iter list
    ~f:(fun (var, exp) ->
	  Printf.fprintf ch "\n";
	  fun_decl var;
	  Printf.fprintf ch "\n{\n";
          let tree1 =
            generate ~opts ~env:[] ~pos_in:"pos_in1" ~pos_out:"pos_out1"
                     ~ok_val:"ok_val1" ~ok:"ok" ~fail:"fail" exp
          in
          let tree =
            T_list
              [T_var "const char *pos_out1";
               T_var "void *ok_val1";
               tree1;
               T_label "ok";
               T_stmt "*pos_out_p = pos_out1;";
               T_stmt "*ok_val_p = ok_val1;";
               T_stmt "return 1;";
               T_label "fail";
               T_stmt "return 0;"]
          in
          print_var_decls ch tree;
          print_body ch tree;
          Printf.fprintf ch "}\n")
