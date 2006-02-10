#include "region.h"
#include "shell.h"
#include "shell-variants.h"

int f_wschar(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_ws(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_tmp0(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_tmp1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_plus(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arrow(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_c_close(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_c_open(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_char(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_opt_str(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_option(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_str_lit2(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_str_lit1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_string_literal(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_pathname_aux(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_pathname(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arg(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arglist4(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arglist3(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arglist2(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_arglist1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_command(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_fail(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);
int f_null(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p);

int f_wschar(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  if(pos_in1 < end && *pos_in1 == ' ') { pos_out1 = pos_in1 + 1; ok_val1 = 0; goto ok; } else goto label0;
 label0:
  if(pos_in1 < end && *pos_in1 == '\t') { pos_out1 = pos_in1 + 1; ok_val1 = 0; goto ok; } else goto label1;
 label1:
  if(pos_in1 < end && *pos_in1 == '\n') { pos_out1 = pos_in1 + 1; ok_val1 = 0; goto ok; } else goto fail;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_ws(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  { const char *pos_out; void *ok_val; if(f_tmp0(r, pos_in1, end, &pos_out, &ok_val)) { pos_out1 = pos_out; ok_val1 = ok_val; goto ok; } else goto fail; }
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_tmp0(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos3;
  void *result5;
  const char *pos10;
  void *result12;
  const char *pos13;
  void *result15;
  const char *pos16;
  void *result18;
  const char *pos6;
  void *result8;
  { const char *pos_out; void *ok_val; if(f_wschar(r, pos_in1, end, &pos_out, &ok_val)) { pos3 = pos_out; result5 = ok_val; goto label4; } else goto label9; }
 label9:
  if(pos_in1 < end && *pos_in1 == '#') { pos10 = pos_in1 + 1; result12 = 0; goto label11; } else goto label2;
 label11:
  { const char *pos_out; void *ok_val; if(f_tmp1(r, pos10, end, &pos_out, &ok_val)) { pos13 = pos_out; result15 = ok_val; goto label14; } else goto label2; }
 label14:
  if(pos13 < end && *pos13 == '\n') { pos16 = pos13 + 1; result18 = 0; goto label17; } else goto label2;
 label17:
  pos3 = pos16;
  { result5 = 0; }
  goto label4;
 label4:
  { const char *pos_out; void *ok_val; if(f_tmp0(r, pos3, end, &pos_out, &ok_val)) { pos6 = pos_out; result8 = ok_val; goto label7; } else goto label2; }
 label7:
  pos_out1 = pos6;
  { void *xs = result8; void *x = result5; ok_val1 = 0; }
  goto ok;
 label2:
  pos_out1 = pos_in1;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_tmp1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos20;
  void *result22;
  const char *pos26;
  void *result28;
  const char *junk_pos32;
  const char *junk_pos33;
  const char *pos29;
  void *result31;
  const char *pos23;
  void *result25;
  if(pos_in1 < end && *pos_in1 == '\n') { junk_pos32 = pos_in1 + 1; junk_pos33 = 0; goto label19; } else goto label34;
 label34:
  pos26 = pos_in1; result28 = 0; goto label27;
 label27:
  if(pos26 < end) { pos29 = pos26 + 1; result31 = (void*) *pos26; goto label30; } else goto label19;
 label30:
  pos20 = pos29;
  { result22 = 0; }
  goto label21;
 label21:
  { const char *pos_out; void *ok_val; if(f_tmp1(r, pos20, end, &pos_out, &ok_val)) { pos23 = pos_out; result25 = ok_val; goto label24; } else goto label19; }
 label24:
  pos_out1 = pos23;
  { void *xs = result25; void *x = result22; ok_val1 = 0; }
  goto ok;
 label19:
  pos_out1 = pos_in1;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_plus(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos35;
  void *result37;
  const char *pos38;
  void *result40;
  if(pos_in1 + 1 <= end && pos_in1[0] == '+') { pos35 = pos_in1 + 1; result37 = 0; goto label36; } else goto fail;
 label36:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos35, end, &pos_out, &ok_val)) { pos38 = pos_out; result40 = ok_val; goto label39; } else goto fail; }
 label39:
  pos_out1 = pos38;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arrow(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos41;
  void *result43;
  const char *pos44;
  void *result46;
  if(pos_in1 + 2 <= end && pos_in1[0] == '=' && pos_in1[1] == '>') { pos41 = pos_in1 + 2; result43 = 0; goto label42; } else goto fail;
 label42:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos41, end, &pos_out, &ok_val)) { pos44 = pos_out; result46 = ok_val; goto label45; } else goto fail; }
 label45:
  pos_out1 = pos44;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_c_close(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos47;
  void *result49;
  const char *pos50;
  void *result52;
  if(pos_in1 < end && *pos_in1 == '}') { pos47 = pos_in1 + 1; result49 = 0; goto label48; } else goto fail;
 label48:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos47, end, &pos_out, &ok_val)) { pos50 = pos_out; result52 = ok_val; goto label51; } else goto fail; }
 label51:
  pos_out1 = pos50;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_c_open(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos53;
  void *result55;
  const char *pos56;
  void *result58;
  if(pos_in1 < end && *pos_in1 == '{') { pos53 = pos_in1 + 1; result55 = 0; goto label54; } else goto fail;
 label54:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos53, end, &pos_out, &ok_val)) { pos56 = pos_out; result58 = ok_val; goto label57; } else goto fail; }
 label57:
  pos_out1 = pos56;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_char(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos110;
  void *result112;
  const char *pos113;
  void *result115;
  const char *pos104;
  void *result106;
  const char *pos107;
  void *result109;
  const char *pos98;
  void *result100;
  const char *pos101;
  void *result103;
  const char *pos92;
  void *result94;
  const char *pos95;
  void *result97;
  const char *pos86;
  void *result88;
  const char *pos89;
  void *result91;
  const char *pos80;
  void *result82;
  const char *pos83;
  void *result85;
  const char *pos65;
  void *result67;
  const char *junk_pos71;
  const char *junk_pos72;
  const char *pos74;
  void *result76;
  const char *pos77;
  void *result79;
  const char *pos68;
  void *result70;
  if(pos_in1 < end && *pos_in1 == '\\') { pos110 = pos_in1 + 1; result112 = 0; goto label111; } else goto label59;
 label111:
  if(pos110 < end && *pos110 == '\\') { pos113 = pos110 + 1; result115 = 0; goto label114; } else goto label59;
 label114:
  pos_out1 = pos113;
  { ok_val1 = (void*) '\\'; }
  goto ok;
 label59:
  if(pos_in1 < end && *pos_in1 == '\\') { pos104 = pos_in1 + 1; result106 = 0; goto label105; } else goto label60;
 label105:
  if(pos104 < end && *pos104 == '\'') { pos107 = pos104 + 1; result109 = 0; goto label108; } else goto label60;
 label108:
  pos_out1 = pos107;
  { ok_val1 = (void*) '\''; }
  goto ok;
 label60:
  if(pos_in1 < end && *pos_in1 == '\\') { pos98 = pos_in1 + 1; result100 = 0; goto label99; } else goto label61;
 label99:
  if(pos98 < end && *pos98 == '"') { pos101 = pos98 + 1; result103 = 0; goto label102; } else goto label61;
 label102:
  pos_out1 = pos101;
  { ok_val1 = (void*) '"'; }
  goto ok;
 label61:
  if(pos_in1 < end && *pos_in1 == '\\') { pos92 = pos_in1 + 1; result94 = 0; goto label93; } else goto label62;
 label93:
  if(pos92 < end && *pos92 == ' ') { pos95 = pos92 + 1; result97 = 0; goto label96; } else goto label62;
 label96:
  pos_out1 = pos95;
  { ok_val1 = (void*) ' '; }
  goto ok;
 label62:
  if(pos_in1 < end && *pos_in1 == '\\') { pos86 = pos_in1 + 1; result88 = 0; goto label87; } else goto label63;
 label87:
  if(pos86 < end && *pos86 == 'n') { pos89 = pos86 + 1; result91 = 0; goto label90; } else goto label63;
 label90:
  pos_out1 = pos89;
  { ok_val1 = (void*) '\n'; }
  goto ok;
 label63:
  if(pos_in1 < end && *pos_in1 == '\\') { pos80 = pos_in1 + 1; result82 = 0; goto label81; } else goto label64;
 label81:
  if(pos80 < end && *pos80 == 't') { pos83 = pos80 + 1; result85 = 0; goto label84; } else goto label64;
 label84:
  pos_out1 = pos83;
  { ok_val1 = (void*) '\t'; }
  goto ok;
 label64:
  if(pos_in1 < end && *pos_in1 == '\\') { pos74 = pos_in1 + 1; result76 = 0; goto label75; } else goto label73;
 label75:
  if(pos74 < end) { pos77 = pos74 + 1; result79 = (void*) *pos74; goto label78; } else goto label73;
 label78:
  junk_pos71 = pos77;
  { junk_pos72 = 0; }
  goto fail;
 label73:
  pos65 = pos_in1; result67 = 0; goto label66;
 label66:
  if(pos65 < end) { pos68 = pos65 + 1; result70 = (void*) *pos65; goto label69; } else goto fail;
 label69:
  pos_out1 = pos68;
  { void *c = result70; ok_val1 = (void*) c; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_opt_str(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos120;
  void *result122;
  const char *junk_pos129;
  const char *junk_pos130;
  const char *pos123;
  void *result125;
  const char *pos126;
  void *result128;
  const char *pos117;
  void *result119;
  { const char *pos_out; void *ok_val; if(f_wschar(r, pos_in1, end, &pos_out, &ok_val)) { junk_pos129 = pos_out; junk_pos130 = ok_val; goto label116; } else goto label131; }
 label131:
  pos120 = pos_in1; result122 = 0; goto label121;
 label121:
  { const char *pos_out; void *ok_val; if(f_char(r, pos120, end, &pos_out, &ok_val)) { pos123 = pos_out; result125 = ok_val; goto label124; } else goto label116; }
 label124:
  { const char *pos_out; void *ok_val; if(f_opt_str(r, pos123, end, &pos_out, &ok_val)) { pos126 = pos_out; result128 = ok_val; goto label127; } else goto label116; }
 label127:
  pos_out1 = pos126;
  { void *rest = result128; void *c = result125; ok_val1 = char_cons(r, (int) c, rest); }
  goto ok;
 label116:
  { const char *pos_out; void *ok_val; if(f_null(r, pos_in1, end, &pos_out, &ok_val)) { pos117 = pos_out; result119 = ok_val; goto label118; } else goto fail; }
 label118:
  pos_out1 = pos117;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_option(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos132;
  void *result134;
  const char *pos135;
  void *result137;
  const char *pos138;
  void *result140;
  if(pos_in1 < end && *pos_in1 == '-') { pos132 = pos_in1 + 1; result134 = 0; goto label133; } else goto fail;
 label133:
  { const char *pos_out; void *ok_val; if(f_opt_str(r, pos132, end, &pos_out, &ok_val)) { pos135 = pos_out; result137 = ok_val; goto label136; } else goto fail; }
 label136:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos135, end, &pos_out, &ok_val)) { pos138 = pos_out; result140 = ok_val; goto label139; } else goto fail; }
 label139:
  pos_out1 = pos138;
  { void *s = result137; ok_val1 = char_cons(r, '-', s); }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_str_lit2(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos145;
  void *result147;
  const char *junk_pos154;
  const char *junk_pos155;
  const char *pos148;
  void *result150;
  const char *pos151;
  void *result153;
  const char *pos142;
  void *result144;
  if(pos_in1 < end && *pos_in1 == '\'') { junk_pos154 = pos_in1 + 1; junk_pos155 = 0; goto label141; } else goto label156;
 label156:
  pos145 = pos_in1; result147 = 0; goto label146;
 label146:
  { const char *pos_out; void *ok_val; if(f_char(r, pos145, end, &pos_out, &ok_val)) { pos148 = pos_out; result150 = ok_val; goto label149; } else goto label141; }
 label149:
  { const char *pos_out; void *ok_val; if(f_str_lit2(r, pos148, end, &pos_out, &ok_val)) { pos151 = pos_out; result153 = ok_val; goto label152; } else goto label141; }
 label152:
  pos_out1 = pos151;
  { void *rest = result153; void *c = result150; ok_val1 = char_cons(r, (int) c, rest); }
  goto ok;
 label141:
  { const char *pos_out; void *ok_val; if(f_null(r, pos_in1, end, &pos_out, &ok_val)) { pos142 = pos_out; result144 = ok_val; goto label143; } else goto fail; }
 label143:
  pos_out1 = pos142;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_str_lit1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos161;
  void *result163;
  const char *junk_pos170;
  const char *junk_pos171;
  const char *pos164;
  void *result166;
  const char *pos167;
  void *result169;
  const char *pos158;
  void *result160;
  if(pos_in1 < end && *pos_in1 == '"') { junk_pos170 = pos_in1 + 1; junk_pos171 = 0; goto label157; } else goto label172;
 label172:
  pos161 = pos_in1; result163 = 0; goto label162;
 label162:
  { const char *pos_out; void *ok_val; if(f_char(r, pos161, end, &pos_out, &ok_val)) { pos164 = pos_out; result166 = ok_val; goto label165; } else goto label157; }
 label165:
  { const char *pos_out; void *ok_val; if(f_str_lit1(r, pos164, end, &pos_out, &ok_val)) { pos167 = pos_out; result169 = ok_val; goto label168; } else goto label157; }
 label168:
  pos_out1 = pos167;
  { void *rest = result169; void *c = result166; ok_val1 = char_cons(r, (int) c, rest); }
  goto ok;
 label157:
  { const char *pos_out; void *ok_val; if(f_null(r, pos_in1, end, &pos_out, &ok_val)) { pos158 = pos_out; result160 = ok_val; goto label159; } else goto fail; }
 label159:
  pos_out1 = pos158;
  { ok_val1 = 0; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_string_literal(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos186;
  void *result188;
  const char *pos189;
  void *result191;
  const char *pos192;
  void *result194;
  const char *pos195;
  void *result197;
  const char *pos174;
  void *result176;
  const char *pos177;
  void *result179;
  const char *pos180;
  void *result182;
  const char *pos183;
  void *result185;
  if(pos_in1 < end && *pos_in1 == '"') { pos186 = pos_in1 + 1; result188 = 0; goto label187; } else goto label173;
 label187:
  { const char *pos_out; void *ok_val; if(f_str_lit1(r, pos186, end, &pos_out, &ok_val)) { pos189 = pos_out; result191 = ok_val; goto label190; } else goto label173; }
 label190:
  if(pos189 < end && *pos189 == '"') { pos192 = pos189 + 1; result194 = 0; goto label193; } else goto label173;
 label193:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos192, end, &pos_out, &ok_val)) { pos195 = pos_out; result197 = ok_val; goto label196; } else goto label173; }
 label196:
  pos_out1 = pos195;
  { void *s = result191; ok_val1 = s; }
  goto ok;
 label173:
  if(pos_in1 < end && *pos_in1 == '\'') { pos174 = pos_in1 + 1; result176 = 0; goto label175; } else goto fail;
 label175:
  { const char *pos_out; void *ok_val; if(f_str_lit2(r, pos174, end, &pos_out, &ok_val)) { pos177 = pos_out; result179 = ok_val; goto label178; } else goto fail; }
 label178:
  if(pos177 < end && *pos177 == '\'') { pos180 = pos177 + 1; result182 = 0; goto label181; } else goto fail;
 label181:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos180, end, &pos_out, &ok_val)) { pos183 = pos_out; result185 = ok_val; goto label184; } else goto fail; }
 label184:
  pos_out1 = pos183;
  { void *s = result179; ok_val1 = s; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_pathname_aux(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos198;
  void *result200;
  const char *pos208;
  void *result210;
  const char *junk_pos214;
  const char *junk_pos215;
  const char *pos211;
  void *result213;
  const char *pos201;
  void *result203;
  const char *pos205;
  void *result207;
  { const char *pos_out; void *ok_val; if(f_wschar(r, pos_in1, end, &pos_out, &ok_val)) { junk_pos214 = pos_out; junk_pos215 = ok_val; goto fail; } else goto label216; }
 label216:
  pos208 = pos_in1; result210 = 0; goto label209;
 label209:
  { const char *pos_out; void *ok_val; if(f_char(r, pos208, end, &pos_out, &ok_val)) { pos211 = pos_out; result213 = ok_val; goto label212; } else goto fail; }
 label212:
  pos198 = pos211;
  { void *c = result213; result200 = (void*) c; }
  goto label199;
 label199:
  { const char *pos_out; void *ok_val; if(f_pathname_aux(r, pos198, end, &pos_out, &ok_val)) { pos201 = pos_out; result203 = ok_val; goto label202; } else goto label204; }
 label204:
  { const char *pos_out; void *ok_val; if(f_null(r, pos198, end, &pos_out, &ok_val)) { pos205 = pos_out; result207 = ok_val; goto label206; } else goto fail; }
 label206:
  pos201 = pos205;
  { void *c = result200; result203 = 0; }
  goto label202;
 label202:
  pos_out1 = pos201;
  { void *rest = result203; void *c = result200; ok_val1 = char_cons(r, (int) c, rest); }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_pathname(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos217;
  void *result219;
  const char *junk_pos226;
  const char *junk_pos227;
  const char *junk_pos229;
  const char *junk_pos230;
  const char *pos220;
  void *result222;
  const char *pos223;
  void *result225;
  if(pos_in1 < end && (('a' <= *pos_in1 && *pos_in1 <= 'z') || ('A' <= *pos_in1 && *pos_in1 <= 'Z') || ('0' <= *pos_in1 && *pos_in1 <= '9') || *pos_in1 == '_' || *pos_in1 == '/' || *pos_in1 == '.' || *pos_in1 == '~')) { junk_pos229 = pos_in1 + 1; junk_pos230 = 0; goto label228; } else goto label231;
 label231:
  junk_pos226 = pos_in1; junk_pos227 = 0; goto fail;
 label228:
  pos217 = pos_in1; result219 = 0; goto label218;
 label218:
  { const char *pos_out; void *ok_val; if(f_pathname_aux(r, pos217, end, &pos_out, &ok_val)) { pos220 = pos_out; result222 = ok_val; goto label221; } else goto fail; }
 label221:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos220, end, &pos_out, &ok_val)) { pos223 = pos_out; result225 = ok_val; goto label224; } else goto fail; }
 label224:
  pos_out1 = pos223;
  { void *s = result222; ok_val1 = s; }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arg(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos240;
  void *result242;
  const char *pos237;
  void *result239;
  const char *pos234;
  void *result236;
  { const char *pos_out; void *ok_val; if(f_option(r, pos_in1, end, &pos_out, &ok_val)) { pos240 = pos_out; result242 = ok_val; goto label241; } else goto label232; }
 label241:
  pos_out1 = pos240;
  { void *s = result242; ok_val1 = mk_arg_string(r, s); }
  goto ok;
 label232:
  { const char *pos_out; void *ok_val; if(f_string_literal(r, pos_in1, end, &pos_out, &ok_val)) { pos237 = pos_out; result239 = ok_val; goto label238; } else goto label233; }
 label238:
  pos_out1 = pos237;
  { void *s = result239; ok_val1 = mk_arg_string(r, s); }
  goto ok;
 label233:
  { const char *pos_out; void *ok_val; if(f_pathname(r, pos_in1, end, &pos_out, &ok_val)) { pos234 = pos_out; result236 = ok_val; goto label235; } else goto fail; }
 label235:
  pos_out1 = pos234;
  { void *f = result236; ok_val1 = mk_arg_filename(r, f); }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arglist4(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos244;
  void *result246;
  const char *pos247;
  void *result249;
  const char *pos250;
  void *result252;
  { const char *pos_out; void *ok_val; if(f_c_open(r, pos_in1, end, &pos_out, &ok_val)) { pos244 = pos_out; result246 = ok_val; goto label245; } else goto label243; }
 label245:
  { const char *pos_out; void *ok_val; if(f_arglist1(r, pos244, end, &pos_out, &ok_val)) { pos247 = pos_out; result249 = ok_val; goto label248; } else goto label243; }
 label248:
  { const char *pos_out; void *ok_val; if(f_c_close(r, pos247, end, &pos_out, &ok_val)) { pos250 = pos_out; result252 = ok_val; goto label251; } else goto label243; }
 label251:
  pos_out1 = pos250;
  { void *l = result249; ok_val1 = l; }
  goto ok;
 label243:
  { const char *pos_out; void *ok_val; if(f_arg(r, pos_in1, end, &pos_out, &ok_val)) { pos_out1 = pos_out; ok_val1 = ok_val; goto ok; } else goto fail; }
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arglist3(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos257;
  void *result259;
  const char *pos260;
  void *result262;
  const char *pos254;
  void *result256;
  { const char *pos_out; void *ok_val; if(f_arglist4(r, pos_in1, end, &pos_out, &ok_val)) { pos257 = pos_out; result259 = ok_val; goto label258; } else goto label253; }
 label258:
  { const char *pos_out; void *ok_val; if(f_arglist3(r, pos257, end, &pos_out, &ok_val)) { pos260 = pos_out; result262 = ok_val; goto label261; } else goto label253; }
 label261:
  pos_out1 = pos260;
  { void *a2 = result262; void *a1 = result259; ok_val1 = mk_arg_cat(r, a1, a2); }
  goto ok;
 label253:
  { const char *pos_out; void *ok_val; if(f_null(r, pos_in1, end, &pos_out, &ok_val)) { pos254 = pos_out; result256 = ok_val; goto label255; } else goto fail; }
 label255:
  pos_out1 = pos254;
  { ok_val1 = mk_arg_empty(r); }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arglist2(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos264;
  void *result266;
  const char *pos267;
  void *result269;
  const char *pos270;
  void *result272;
  { const char *pos_out; void *ok_val; if(f_arglist3(r, pos_in1, end, &pos_out, &ok_val)) { pos264 = pos_out; result266 = ok_val; goto label265; } else goto label263; }
 label265:
  { const char *pos_out; void *ok_val; if(f_plus(r, pos264, end, &pos_out, &ok_val)) { pos267 = pos_out; result269 = ok_val; goto label268; } else goto label263; }
 label268:
  { const char *pos_out; void *ok_val; if(f_arglist2(r, pos267, end, &pos_out, &ok_val)) { pos270 = pos_out; result272 = ok_val; goto label271; } else goto label263; }
 label271:
  pos_out1 = pos270;
  { void *fs = result272; void *a = result266; ok_val1 = mk_arg_cat(r, a, mk_arg_ambient(r, fs)); }
  goto ok;
 label263:
  { const char *pos_out; void *ok_val; if(f_arglist3(r, pos_in1, end, &pos_out, &ok_val)) { pos_out1 = pos_out; ok_val1 = ok_val; goto ok; } else goto fail; }
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_arglist1(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos274;
  void *result276;
  const char *pos277;
  void *result279;
  const char *pos280;
  void *result282;
  { const char *pos_out; void *ok_val; if(f_arglist2(r, pos_in1, end, &pos_out, &ok_val)) { pos274 = pos_out; result276 = ok_val; goto label275; } else goto label273; }
 label275:
  { const char *pos_out; void *ok_val; if(f_arrow(r, pos274, end, &pos_out, &ok_val)) { pos277 = pos_out; result279 = ok_val; goto label278; } else goto label273; }
 label278:
  { const char *pos_out; void *ok_val; if(f_arglist2(r, pos277, end, &pos_out, &ok_val)) { pos280 = pos_out; result282 = ok_val; goto label281; } else goto label273; }
 label281:
  pos_out1 = pos280;
  { void *a2 = result282; void *a1 = result276; ok_val1 = mk_arg_cat(r, mk_arg_read(r, a1), mk_arg_write(r, a2)); }
  goto ok;
 label273:
  { const char *pos_out; void *ok_val; if(f_arglist2(r, pos_in1, end, &pos_out, &ok_val)) { pos_out1 = pos_out; ok_val1 = ok_val; goto ok; } else goto fail; }
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_command(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  const char *pos297;
  void *result299;
  const char *pos300;
  void *result302;
  const char *pos303;
  void *result305;
  const char *pos291;
  void *result293;
  const char *pos294;
  void *result296;
  const char *pos285;
  void *result287;
  const char *pos288;
  void *result290;
  if(pos_in1 + 2 <= end && pos_in1[0] == 'c' && pos_in1[1] == 'd') { pos297 = pos_in1 + 2; result299 = 0; goto label298; } else goto label283;
 label298:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos297, end, &pos_out, &ok_val)) { pos300 = pos_out; result302 = ok_val; goto label301; } else goto label283; }
 label301:
  { const char *pos_out; void *ok_val; if(f_pathname(r, pos300, end, &pos_out, &ok_val)) { pos303 = pos_out; result305 = ok_val; goto label304; } else goto label283; }
 label304:
  pos_out1 = pos303;
  { void *dir = result305; ok_val1 = mk_chdir(r, dir); }
  goto ok;
 label283:
  if(pos_in1 + 2 <= end && pos_in1[0] == 'c' && pos_in1[1] == 'd') { pos291 = pos_in1 + 2; result293 = 0; goto label292; } else goto label284;
 label292:
  { const char *pos_out; void *ok_val; if(f_ws(r, pos291, end, &pos_out, &ok_val)) { pos294 = pos_out; result296 = ok_val; goto label295; } else goto label284; }
 label295:
  pos_out1 = pos294;
  { ok_val1 = mk_chdir(r, char_cons(r, '~', 0)); }
  goto ok;
 label284:
  { const char *pos_out; void *ok_val; if(f_pathname(r, pos_in1, end, &pos_out, &ok_val)) { pos285 = pos_out; result287 = ok_val; goto label286; } else goto fail; }
 label286:
  { const char *pos_out; void *ok_val; if(f_arglist1(r, pos285, end, &pos_out, &ok_val)) { pos288 = pos_out; result290 = ok_val; goto label289; } else goto fail; }
 label289:
  pos_out1 = pos288;
  { void *a = result290; void *c = result287; ok_val1 = mk_command(r, c, a); }
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_fail(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  goto fail;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}

int f_null(region_t r, const char *pos_in1, const char *end, const char **pos_out_p, void **ok_val_p)
{
  const char *pos_out1;
  void *ok_val1;
  pos_out1 = pos_in1;
  ok_val1 = 0;
  goto ok;
 ok:
  *pos_out_p = pos_out1;
  *ok_val_p = ok_val1;
  return 1;
 fail:
  return 0;
}
