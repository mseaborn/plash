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

type p_expr =
  | P_var of string (* name of non-terminal *)
  | P_string of string
  | P_char_class of (char * char) list (* list of inclusive ranges that are accepted *)
  | P_seq of p_expr * p_expr
  | P_seqm of (bool * p_expr) list * string (* builds a tree node *)
  | P_choice of p_expr * p_expr
  | P_repeat of p_expr
  | P_not of p_expr
  | P_null
  | P_fail
  | P_strnode of p_expr (* sets value to string matched *)

(*

exp =
    ^var open ^args close => app
  / ^var => var

args =
    ^exp (comma ^exp)*
  / null

open = ws '('
close = ws ')'
comma = ws ','
var = ws ^[a-zA-Z_] ^[a-zA-Z_0-9]*

ws = (' ' / '\t' / '\n')*

*)


let p_char c = P_char_class [(c, c)]
let rec p_choice = function
  | [] -> P_fail
  | [exp] -> exp
  | exp::rest -> P_choice(exp, p_choice rest)
let rec p_seql = function
  | [] -> P_null
  | [exp] -> exp
  | exp::rest -> P_seq(exp, p_seql rest)
let p_seq list name = P_seqm(list, name)
let p_seq0 = p_seql

let fresh_id =
  let cell = ref 0 in
    (fun() ->
       let i = !cell in
         cell := i+1; "tmp"^(string_of_int i))

let gram = ref []

let p_repeat exp =
  let name = fresh_id() in
  gram := (name,
           p_choice [p_seq [(true, exp);
                            (true, P_var name)] "cons";
                     P_null])::!gram;
  P_var name

let grammar =
[("exp",
  p_choice
    [p_seq [(true, P_var "var");
            (false, P_var "open");
            (true, P_var "args");
            (false, P_var "close")] "exp_app";
     p_seq [(true, P_var "var")] "exp_var";
    ]);

 ("args",
  p_choice
    [p_seq [(true, P_var "exp");
            (true, p_repeat (p_seq [(false, P_var "comma");
                                    (true, P_var "exp")] "singleton"))]
         "cons";
     P_null;
    ]);

 ("open",
  p_seq0 [P_var "ws"; p_char '(']);
 ("close",
  p_seq0 [P_var "ws"; p_char ')']);
 ("comma",
  p_seq0 [P_var "ws"; p_char ',']);

 ("var",
  P_strnode
   (p_seq [(false, P_var "ws");
         (true, P_char_class [('a','z'); ('A','Z'); ('_','_')]);
         (true, p_repeat (P_char_class [('a','z'); ('A','Z'); ('_','_'); ('0','9')]));
        ] "var"));

 ("ws", p_repeat (P_char_class [(' ',' '); ('\t','\t'); ('\n','\n')]));
]
let grammar = grammar @ !gram

(* Map non-terminal names to indexes *)
let nt_indexes = Hashtbl.create 101
let no_nts =
  List.fold_left grammar ~init:0
    ~f:(fun i (name, _) -> Hashtbl.add nt_indexes name i; i+1)
let nt_defs = Array.map ~f:(fun (_, exp) -> exp) (Array.of_list grammar)

let text = "foo(bar(qux, a, b, c, f(y,z), h(k)), j, k(f(i)))"

type parse_tree =
  | T of string * parse_tree array
let null = T("null", [||])

(* It's better for Parse to return the index consumed upto, rather than
   the number of characters read, because this lets P_seq do a tail call. *)
type result =
  | Parsed of int (* index consumed upto *) * parse_tree
  | Fail
type result1 = {
  parse : result;
  (* Index that has been read upto; if it encountered the end of the file,
     this should be the file length + 1.  It's as if there is an end of
     file symbol. *)
  lookahead : int;
}

let memo =
  Array.init (String.length text) ~f:(fun _ -> Array.make no_nts None)

(* Returns the max of the lookahead this actually does, and the `lookahead'
   argument.  This means that we can do tail calls for seq and choice. *)
let rec parse ~i ~lookahead = function
  | P_var v -> parse_nt ~i ~lookahead v
  | P_string str ->
      let rec f j =
        if j = String.length str then
          { parse = Parsed (i+j, T(str, [||]));
            lookahead = max lookahead (i+j) }
        else
        if i+j < String.length text && str.[j] = text.[i+j]
          then f (j+1)
          else { parse = Fail; lookahead = max lookahead (i+j+1) }
      in f 0
  | P_char_class cl ->
      let rec in_class c = function
	| (min, max)::rest -> (min <= c && c <= max) || in_class c rest
	| [] -> false
      in
        { parse = (if i < String.length text && in_class text.[i] cl
                   then Parsed (i+1, null) else Fail);
          lookahead = max lookahead (i+1) }
  | P_seq(e1, e2) ->
      let r = parse ~i ~lookahead e1 in 
        (match r.parse with
           | Parsed (j, _) -> parse ~i:j ~lookahead:r.lookahead e2
           | Fail -> { parse = Fail; lookahead = r.lookahead })
  | P_seqm(list, name) ->
      let rec f ~i ~lookahead got = function
        | [] ->
	    { parse = Parsed(i, T(name, Array.of_list (List.rev got)));
	      lookahead = lookahead }
        | (keep, exp)::rest ->
            let r = parse ~i ~lookahead exp in
              match r.parse with
                | Parsed(j, tree) ->
	            f ~i:j ~lookahead:r.lookahead
		      (if keep then tree::got else got) rest
                | Fail -> { parse = Fail; lookahead = lookahead }
      in f ~i ~lookahead [] list
  | P_choice(e1, e2) ->
      let r = parse ~i ~lookahead e1 in 
        (match r.parse with
           | Parsed _ -> r
           | Fail -> parse ~i ~lookahead:r.lookahead e2)
  | P_null -> { parse = Parsed (i, null); lookahead = lookahead }
  | P_fail -> { parse = Fail; lookahead = lookahead }
  | P_strnode(e) ->
      let r = parse ~i ~lookahead e in
        { lookahead = r.lookahead;
          parse =
            match r.parse with
              | Parsed(j, _) ->
	          Parsed(j, T(String.sub text ~pos:i ~len:(j-i), [||]))
	      | Fail -> Fail }

and parse_nt ~i ~lookahead v =
  let index = Hashtbl.find nt_indexes v in
    match memo.(i).(index) with
      | Some r ->
          { parse = r.parse; lookahead = max lookahead r.lookahead }
      | None ->
          let r = parse ~i ~lookahead:i nt_defs.(index) in
            memo.(i).(index) <- Some r;
            { parse = r.parse; lookahead = max lookahead r.lookahead }

(*
type inc_info = {
  text : string;
  text2 : change_tree;
  memo : ...;
  memo2 : ..;
}

let rec parse_inc ~memo1 ~memo2 ~text ~i ~lookahead pexp =
  let next = parse_inc ~memo1 ~memo2 ~text in
  match pexp with
    | 
*)

let r = parse_nt ~i:0 ~lookahead:0 "exp"

let las =
  Array.mapi memo
    ~f:(fun i a ->
	  Array.map a ~f:(function
			    | Some { lookahead=j } -> Some (j-i)
			    |_ -> None))


(* incremental parsing:

   map new string onto ranges of the old string.
   cache lookup maps index to index in old string, looks up, checks range.
    * record into new cache.

   if successful, combine old and new cache


   changes can be applied in any order -- need to deal with this


   algo:
   - find the smallest tree that covers all the changes
   - reparse that tree, ie. parse the same non-terminal at the same start pos
   er, hmm...

   strategy:
   starting from the smallest subtrees that contain changes, reparse those
   subtrees.
    * if it succeeds, it's a good sign that the change can be integrated
    * if it fails, try the next subtree up

   for every node, we want to try:
    * integrating all the changes in it
    * integrating all the changes that integrate successfully into their
      separate subtrees
   suggests:
    * start at top
    * handle(tree) returns a set of changes that can be integrated for a tree
    * handle(tree):
        try to reparse tree with all changes (set C) integrated
	(but with no other changes integrated -- hence right lookahead
	is same as before other changes)
	[an alternative would be to reparse tree with EOF following its text]
	 if OK, return all changes
	 otherwise,
	   call handle() on all subtrees
	   consider the union U of the changes they return
           -- this can be less than C, because there are changes that
	   don't fall under a particular subtree
	   * try to reparse tree with all U integrated
	   return U if successful
	   otherwise return empty set
    * On the way down, we are not changing the changes that we're applying
      at all.  We can reuse any memoised results in this case.
      We could implement handle() breadth-first, rather than the more
      obvious depth-first version suggested above.
    * Then we change which changes we're applying once.
      But on the way up, we don't change them further.

   Random notes:
    * We could have placeholder symbols, eg. <Exp>, which the non-terminal
      Exp will match, but nothing else will, even the character wildcard `.'.
      Nice and unambiguous.
      Problem:  "f <Exp>" wouldn't be correctly parsed as a function
      application, because skipping whitespace is done by the lexer
      tokens, and <Exp> is not being matched by a lexer token...
*)

type change =
  | C_original of int
  | C_insert of string
  | C_delete of int

(*
let cons = function
  | (C_insert s1, (C_insert s2)::rest) -> (C_insert (s1^s2))::rest
  | (C_delete n1, (C_delete n2)::rest) -> (C_delete (n1^n2))::rest
  | (C_original 

let rec insert i str = function
  | C_original n 
*)


(* How to bootstrap a parser generator?
   
def = var "=" pexp

pexp = pexp1 ("/" pexp1)*
pexp1 = pexp2+
pexp2 = pexp3 ("*"/"+")?
pexp3 =
    "(" pexp ")"
  / var
  / string

built-in vars are "null" and "fail"
