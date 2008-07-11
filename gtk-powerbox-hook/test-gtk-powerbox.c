/* Copyright (C) 2007 Mark Seaborn

   This file is part of Plash.

   Plash is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   Plash is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Plash; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
   USA.  */

#include <assert.h>
#include <string.h>

#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmain.h>


void test_open_file()
{
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("File open test",
				NULL /* parent */,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
  g_object_ref_sink(dialog);
  gtk_widget_show(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  assert(response == GTK_RESPONSE_ACCEPT);

  char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  assert(filename != NULL);
  assert(strcmp(filename, "/home/foo/example-pathname") == 0);
  g_free(filename);
  gtk_widget_unref(dialog);
}

void test_save_file()
{
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("File save test",
				NULL /* parent */,
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
  g_object_ref_sink(dialog);
  gtk_widget_show(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  assert(response == GTK_RESPONSE_ACCEPT);

  char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  assert(filename != NULL);
  assert(strcmp(filename, "/home/foo/example-pathname") == 0);
  g_free(filename);
  gtk_widget_unref(dialog);
}

void test_cancellation()
{
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("File open test",
				NULL /* parent */,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
  g_object_ref_sink(dialog);
  gtk_widget_show(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  assert(response == GTK_RESPONSE_CANCEL);
  gtk_widget_unref(dialog);
}

void test_parent_window_id()
{
  GtkWidget *parent_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink(parent_window);
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("File open test",
				GTK_WINDOW(parent_window),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
  g_object_ref_sink(dialog);
  gtk_widget_realize(parent_window);
  gtk_widget_show(dialog);
  gtk_widget_unref(dialog);
  gtk_widget_unref(parent_window);
}

void test_multiple_requests()
{
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("Directory open test",
				NULL /* parent window */,
				GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
  g_object_ref_sink(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  /* Should this test work without the gtk_widget_hide() call as well? */
  gtk_widget_hide(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_unref(dialog);
}

void test_success_response_code()
{
  GtkWidget *dialog =
    gtk_file_chooser_dialog_new("File open test",
				NULL /* parent */,
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				"Some label, OK?", GTK_RESPONSE_OK,
				"Some label, yes?", GTK_RESPONSE_YES,
				NULL);
  g_object_ref_sink(dialog);
  gtk_widget_show(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  assert(response == GTK_RESPONSE_YES);
  gtk_widget_unref(dialog);
}


struct test_case {
  const char *name;
  void (*func)(void);
};

#define TEST_CASE(name) { #name, name }

struct test_case test_cases[] =
{
  TEST_CASE(test_open_file),
  TEST_CASE(test_save_file),
  TEST_CASE(test_cancellation),
  TEST_CASE(test_parent_window_id),
  TEST_CASE(test_multiple_requests),
  TEST_CASE(test_success_response_code),
  { NULL, NULL }
};


int main(int argc, char **argv)
{
  gtk_init(&argc, &argv);
  if(argc != 2) {
    fprintf(stderr, "Usage: %s [test-case-name]\n", argv[0]);
    return 1;
  }

  struct test_case *test_case;
  for(test_case = test_cases; test_case->name != NULL; test_case++) {
    if(strcmp(test_case->name, argv[1]) == 0) {
      test_case->func();
      return 0;
    }
  }
  fprintf(stderr, "Test case not known: \"%s\"\n", argv[1]);
  return 1;
}
