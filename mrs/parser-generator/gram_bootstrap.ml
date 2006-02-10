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


open Pexprs


let defs = ref []
let add_def name pexp = defs := (name, pexp)::!defs
let fresh_name =
  let c = ref 0 in
    (fun()-> let i = !c in c := i+1; "tmp"^(string_of_int i))

let p_seq(list, map_expr) = P_seq(list, Some map_expr)
(* These implementations of p_repeat and p_opt are somewhat unnecessary
   now that I've added `simplify' to transform away P_repeat etc., but
   I'll leave them in anyway. *)
let p_repeat exp =
  let var = fresh_name() in
    add_def var
      (P_choice(p_seq([(exp, Some "x");
		       (P_var var, Some "xs")],
		      "x::xs"),
		p_seq([(P_null, None)], "[]")));
    P_var var
let p_opt exp =
  P_choice (p_seq([(exp, Some "x")], "Some x"),
            p_seq([(P_null, None)], "None"))


let _ =
  add_def "defs"
    (p_seq([(P_var "ws", None);
	    (p_repeat (P_var "def"), Some "x");
            (P_end, None)],
	   "x"));

  add_def "def"
    (p_seq([(P_var"var", Some "v");
            (P_var"equals", None);
            (P_var"pexp1", Some "exp");
            (P_var"semicolon", None)],
           "(v, exp)"));

  add_def "pexp1"
    (p_seq([(P_var"pexp2", Some "e");
	    (p_repeat
	       (p_seq([(P_char '/', None);
		       (P_var "ws", None);
		       (P_var "pexp2", Some "e")],
		      "e")),
	     Some "es")],
	   "reduce_right (e::es) ~f:(fun e1 e2 -> P_choice(e1, e2)) ~id:P_fail"));

  add_def "pexp2"
    (p_seq([(P_var"pexp3", Some "e");
	    (p_repeat (P_var"pexp3"), Some "es");
	    (p_opt (p_seq([(P_var "sem_open", None);
			   (p_repeat
			      (p_seq
			       ([(P_not
				   (p_seq([(P_var "ws", None);
				           (P_var "sem_close", None)], "()")),
				  None);
				 (P_char_any, Some "c")],
				"c")),
			    Some "text");
                           (P_var "ws", None);
			   (P_var "sem_close", None)],
			  "string_of_list text")),
             Some "mexp")],
	    "match (e::es, mexp) with\
               | ([(exp, None)], None) -> exp\
               | (list, _) -> P_seq(list, mexp)"));

  add_def "pexp3"
    (P_choice
      (p_seq([(P_var "var", Some "v");
	      (P_var "colon", None);
	      (P_var "pexp4", Some "exp")],
	      "(exp, Some v)"),
       p_seq([(P_var "pexp4", Some "exp")],
	     "(exp, None)")));

  add_def "pexp4"
    (P_choice
      (p_seq([(P_var "neg", None);
              (P_var "pexp5", Some "e")],
	     "P_not(e)"),
    (P_choice
      (p_seq([(P_var "pexp5", Some "e");
	      (P_var "star", None)],
	     "P_repeat(e)"),
    (P_choice
      (p_seq([(P_var "pexp5", Some "e");
	      (P_var "plus", None)],
	     "P_repeatplus(e)"),
    (P_choice
      (p_seq([(P_var "pexp5", Some "e");
	      (P_var "question", None)],
	     "P_opt(e)"),
       P_var "pexp5"))))))));

  add_def "pexp5"
    (P_choice(P_var "char_literal",
     P_choice(P_var "char_class",
     P_choice(P_var "char_any",
     P_choice(p_seq([(P_var "var", Some "v")], "P_var(v)"),
     P_choice(p_seq([(P_var "open", None);
	             (P_var "pexp1", Some "exp");
	             (P_var "close", None)],
	            "exp"),
              p_seq([(P_char '\\', None);
                     (P_char 's', None);
                     (P_char 't', None);
                     (P_char 'r', None);
                     (P_char 'i', None);
                     (P_char 'n', None);
                     (P_char 'g', None);
                     (P_var "open", None);
		     (P_var "pexp1", Some "e");
		     (P_var "close", None)],
                    "P_get_string(e)")))))));

  add_def "char_literal"
    (p_seq([(P_char '\'', None);
	    (P_var "char", Some "c");
	    (P_char '\'', None);
	    (P_var "ws", None)],
	   "P_char(c)"));

  add_def "char"
    (P_choice(p_seq([(P_char '\\', None);
		     (P_char '\\', None)],
		    "'\\\\'"),
    (P_choice(p_seq([(P_char '\\', None);
		     (P_char '\'', None)],
		    "'\\''"),
    (P_choice(p_seq([(P_char '\\', None);
		     (P_char 'n', None)],
		    "'\\n'"),
    (P_choice(p_seq([(P_char '\\', None);
		     (P_char 't', None)],
		    "'\\t'"),
       P_char_any))))))));

  add_def "char_class"
    (p_seq([(P_char '[', None);
            (p_repeat
               (P_choice
                  (p_seq([(P_var "char2", Some "c1");
                          (P_char '-', None);
                          (P_var "char2", Some "c2")],
                         "(c1, c2)"),
                   p_seq([(P_var "char2", Some "c")],
                         "(c, c)"))),
             Some "list");
            (P_char ']', None);
            (P_var "ws", None)],
           "P_char_class(list)"));
  add_def "char2"
    (p_seq([(P_not (P_char ']'), None);
            (P_var "char", Some "c")],
           "c"));

  add_def "char_any"
    (p_seq([(P_char '.', None);
	    (P_var "ws", None)],
	   "P_char_any"));

  add_def "var"
    (p_seq([(P_get_string
              (p_seq([(P_char_class [('a','z'); ('A','Z'); ('_','_')], None);
                      (p_repeat (P_char_class [('a','z'); ('A','Z'); ('_','_'); ('0','9')]), None)],
                     "()")),
             Some "v");
            (P_var "ws", None)],
	   "v"));

  add_def "open" (p_seq([(P_char '(', None); (P_var"ws", None)], "()"));
  add_def "close" (p_seq([(P_char ')', None); (P_var"ws", None)], "()"));
  add_def "sem_open" (p_seq([(P_char '{', None); (P_char '{', None); (P_var"ws", None)], "()"));
  add_def "sem_close" (p_seq([(P_char '}', None); (P_char '}', None); (P_var"ws", None)], "()"));

  add_def "equals" (p_seq([(P_char '=', None); (P_var"ws", None)], "()"));
  add_def "semicolon" (p_seq([(P_char ';', None); (P_var"ws", None)], "()"));
  add_def "colon" (p_seq([(P_char ':', None); (P_var"ws", None)], "()"));
  add_def "star" (p_seq([(P_char '*', None); (P_var"ws", None)], "()"));
  add_def "plus" (p_seq([(P_char '+', None); (P_var"ws", None)], "()"));
  add_def "question" (p_seq([(P_char '?', None); (P_var"ws", None)], "()"));
  add_def "neg" (p_seq([(P_char '!', None); (P_var"ws", None)], "()"));
  add_def "ws" (p_repeat (P_char_class [(' ',' '); ('\t','\t'); ('\n','\n')]))



let _ =
  let ch = if false then open_out "gram_bootstrap_out.ml" else stdout in
    output_string ch "\nopen StdLabels\nopen My_utils\nopen Pexprs\n";
    Generator.output ch (List.rev !defs)
