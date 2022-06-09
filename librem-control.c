/*
 * Copyright (C) 2022 Nicole Faerber <nicole.faerber@puri.sm>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

//#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "ec-tool.h"

#define LED_RED_PATH			"/sys/class/leds/red:status"
#define LED_GREEN_PATH			"/sys/class/leds/green:status"
#define LED_BLUE_PATH			"/sys/class/leds/blue:status"
#define LED_AIRPLANE_PATH		"/sys/class/leds/librem_ec:airplane"
#define LED_KBD_BACKLIGHT		"/sys/class/leds/librem_ec:kbd_backlight"

#define BAT_SOC					"/sys/class/power_supply/BAT0/capacity"
#define BAT_START_THRESHOLD_PATH	"/sys/class/power_supply/BAT0/charge_control_start_threshold"
#define BAT_END_THRESHOLD_PATH		"/sys/class/power_supply/BAT0/charge_control_end_threshold"

#define CPU_PL1_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_0_power_limit_uw"
#define CPU_PL2_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_1_power_limit_uw"

#define BIOS_DMI_PATH			"/sys/class/dmi/id/"
#define BIOS_DMI_PRODUCT_NAME	"product_name"
#define BIOS_DMI_BIOS_VERSION	"bios_version"
#define BIOS_DMI_BIOS_DATE		"bios_date"
#define BIOS_DMI_BOARD_SERIAL	"board_serial"

// CometLake U, TDP 15W, cTDP-Up 25W
// Intel recommends: PL2 = PL1 * 1.25, would be 18.75W
// some Intel NUC BIOS set this to 30/40 !?

// /sys/class/dmi/id/
// bios_version
// bios_date
// board_serial
// product_name

typedef struct {
	GtkWidget *window;
	GtkApplication *gapp;
	gboolean is_root;
	double bat_soc;
	GtkWidget *bat_soc_pbar;
	GtkWidget *bat_start_slider;
	double bat_start_thres;
	GtkWidget *bat_end_slider;
	double bat_end_thres;
	GtkWidget *bat_apply_btn;
	GtkWidget *bat_undo_btn;
	double cpu_pl1;
	GtkWidget *cpu_pl1_slider;
	double cpu_pl2;
	GtkWidget *cpu_pl2_slider;
	GtkWidget *cpu_apply_btn;
	GtkWidget *cpu_undo_btn;
	int kbd_backl;
	GtkWidget *rfkill_tbtn1;
	GtkWidget *rfkill_tbtn2;
	GtkWidget *rfkill_tbtn3;
	bool airplane;
	int red_val;
	int green_val;
	int blue_val;
} lcontrol_app_t ;


static int get_string_from_text_file(char *fname, char *string, int len)
{
	FILE *fp;
	int res;

	if (len < 2)
		return -1;

	fp=fopen(fname, "r");

	if (fp==NULL) {
		perror(fname);
		return -1;
	}

	memset(string, 0, len);

	res = fread(string, 1, len-1, fp);
	if (res <= 0) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	if (string[res-1] == '\n')
		string[res-1] = 0;

	return res;
}

static int get_value_from_text_file(char *fname)
{
	FILE *fp;
	char buf[64];

	fp=fopen(fname, "r");

	if (fp==NULL) {
		perror(fname);
		return -1;
	}

	memset(buf, 0, 64);

	if (fread(buf, 1, 63, fp) <= 0) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return atoi(buf);
}

static int set_value_to_text_file(char *fname, char *value)
{
	int fd;

	// fprintf(stderr, "set_value_to_text_file('%s', '%s')\n", fname, value);
	if (value == NULL)
		return -EINVAL;

	fd = open(fname, O_WRONLY);
	if (fd < 0)
		return fd;
	write(fd, value, strlen(value));
	close(fd);

	return 1;
}

static void update_values_get(lcontrol_app_t *lc_app)
{
	int val;

	val = get_value_from_text_file(BAT_SOC);
	if (val >= 0)
		lc_app->bat_soc = (double)val;
	val = get_value_from_text_file(BAT_START_THRESHOLD_PATH);
	if (val >= 0)
		lc_app->bat_start_thres = (double)val;
	val = get_value_from_text_file(BAT_END_THRESHOLD_PATH);
	if (val >= 0)
		lc_app->bat_end_thres = (double)val;

	val = get_value_from_text_file(CPU_PL1_PATH);
	if (val >= 0)
		lc_app->cpu_pl1 = (double)val / 1000000;
	val = get_value_from_text_file(CPU_PL2_PATH);
	if (val >= 0)
		lc_app->cpu_pl2 = (double)val / 1000000;

	val = get_value_from_text_file(LED_RED_PATH "/brightness");
	if (val >= 0)
		lc_app->red_val = val;
	val = get_value_from_text_file(LED_GREEN_PATH "/brightness");
	if (val >= 0)
		lc_app->green_val = val;
	val = get_value_from_text_file(LED_BLUE_PATH "/brightness");
	if (val >= 0)
		lc_app->blue_val = val;

	val = get_value_from_text_file(LED_KBD_BACKLIGHT "/brightness");
	if (val >= 0)
		lc_app->kbd_backl = val;

	val = get_value_from_text_file(LED_AIRPLANE_PATH "/brightness");
	if (val >= 0)
		lc_app->airplane = (val > 0) ? true : false;
}


static void bat_start_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
    double bat_start_val;
    double bat_end_val;

    bat_end_val = gtk_range_get_value(GTK_RANGE(lc_app->bat_end_slider));
    bat_start_val = gtk_range_get_value(self);

    if (bat_start_val < 10) {
        bat_start_val = 10;
        gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_start_val);
    }
    if (bat_start_val > 99) {
        bat_start_val = 99;
        gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_start_val);
    }
    bat_start_val+=1;
    if (bat_start_val > bat_end_val) {
        gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), bat_start_val);
    }

    if (lc_app->is_root) {
		gtk_widget_set_sensitive(lc_app->bat_apply_btn, true);
		gtk_widget_set_sensitive(lc_app->bat_undo_btn, true);
	}
}

static void bat_end_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
    double bat_start_val;
    double bat_end_val;

	bat_start_val = gtk_range_get_value(GTK_RANGE(lc_app->bat_start_slider));
	bat_end_val = gtk_range_get_value(self);

	bat_end_val-=1;
	if (bat_end_val < bat_start_val) {
		gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), bat_end_val);
    }

    if (lc_app->is_root) {
		gtk_widget_set_sensitive(lc_app->bat_apply_btn, true);
		gtk_widget_set_sensitive(lc_app->bat_undo_btn, true);
	}
}

static void bat_thres_apply_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	char buf[32];

    if (lc_app->is_root) {
		lc_app->bat_end_thres = gtk_range_get_value(GTK_RANGE(lc_app->bat_end_slider));
		lc_app->bat_start_thres = gtk_range_get_value(GTK_RANGE(lc_app->bat_start_slider));

		snprintf(buf, 31, "%d", (int)lc_app->bat_start_thres);
		set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);

		snprintf(buf, 31, "%d", (int)lc_app->bat_end_thres);
		set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);
	}

	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
}

static void bat_thres_undo_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (lc_app->is_root) {
    	gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), lc_app->bat_start_thres);
    	gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), lc_app->bat_end_thres);
	}

	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
}

static void start_charge_now_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	int tval;
	char buf[32];

	// only works if SOC < end threshold
	if (lc_app->bat_end_thres < lc_app->bat_soc) {
		fprintf(stderr, "not starting, end %d, soc %d\n", (int)lc_app->bat_end_thres, (int)lc_app->bat_soc);
		return;
	}
	tval = 	(int)lc_app->bat_end_thres - 1;
	snprintf(buf, 31, "%d", tval);
	set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);
	g_usleep(G_USEC_PER_SEC + (G_USEC_PER_SEC / 4));
	snprintf(buf, 31, "%d", (int)lc_app->bat_start_thres);
	set_value_to_text_file(BAT_START_THRESHOLD_PATH, buf);
}

static void stop_charge_now_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	int tval;
	char buf[32];

	// only works if start threshold < SOC
	if (lc_app->bat_start_thres >= lc_app->bat_soc) {
		return;
	}
	tval = 	(int)lc_app->bat_soc + 1;
	snprintf(buf, 31, "%d", tval);
	set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);
	g_usleep(G_USEC_PER_SEC + (G_USEC_PER_SEC / 4));
	snprintf(buf, 31, "%d", (int)lc_app->bat_end_thres);
	set_value_to_text_file(BAT_END_THRESHOLD_PATH, buf);

}

static void cpu_pl1_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, true);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, true);
}

static void cpu_pl2_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, true);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, true);
}

static void cpu_undo_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (lc_app->is_root) {
    	gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl1_slider), lc_app->cpu_pl1);
    	gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl2_slider), lc_app->cpu_pl2);
	}

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, false);
}

static void cpu_apply_clicked (GtkWidget *widget, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	char buf[32];

    if (lc_app->is_root) {
    	lc_app->cpu_pl1 = gtk_range_get_value(GTK_RANGE(lc_app->cpu_pl1_slider));
    	lc_app->cpu_pl2 = gtk_range_get_value(GTK_RANGE(lc_app->cpu_pl2_slider));

		snprintf(buf, 31, "%d", (int)lc_app->cpu_pl1 * 1000000);
		set_value_to_text_file(CPU_PL1_PATH, buf);
		snprintf(buf, 31, "%d", (int)lc_app->cpu_pl2 * 1000000);
		set_value_to_text_file(CPU_PL2_PATH, buf);
	}

	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, false);
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, false);
}

static void kbd_backl_val_chg (GtkRange* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	char buf[32];

	lc_app->kbd_backl = gtk_range_get_value(self);
	snprintf(buf, 31, "%d", lc_app->kbd_backl);
	set_value_to_text_file(LED_KBD_BACKLIGHT "/brightness", buf);
}


static void led_rfkill_toggled (GtkCheckButton* self, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

	if (!gtk_check_button_get_active(self)) {
		return;
	} else {
		if (GTK_WIDGET(self) == lc_app->rfkill_tbtn1) {
			set_value_to_text_file(LED_AIRPLANE_PATH "/trigger", "rfkill-none");
		};
		if (GTK_WIDGET(self) == lc_app->rfkill_tbtn2) {
			set_value_to_text_file(LED_AIRPLANE_PATH "/trigger", "phy0rx");
		};
		if (GTK_WIDGET(self) == lc_app->rfkill_tbtn3) {
			set_value_to_text_file(LED_AIRPLANE_PATH "/trigger", "phy0tx");
		};
	}
}

gboolean update_values_timer(gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;
	int val;

	val = get_value_from_text_file(BAT_SOC);
	if (val >= 0) {
		lc_app->bat_soc = (double)val;
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lc_app->bat_soc_pbar), lc_app->bat_soc / 100.);
	}
	return G_SOURCE_CONTINUE;
}

static void close_window (gpointer user_data)
{
	//lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    // clean up and quit
	//gtk_application_remove_window(lc_app->gapp, GTK_WINDOW(lc_app->window));
}


void create_main_window (lcontrol_app_t *lc_app)
{
    GtkWidget *box;
    GtkWidget *w, *c;
	GtkWidget *stack;
	GtkWidget *stack_sb;

    gtk_window_set_title (GTK_WINDOW (lc_app->window), "Librem Control");
	gtk_window_set_default_size(GTK_WINDOW(lc_app->window), 400, 300);

    g_signal_connect (lc_app->window, "destroy",
        G_CALLBACK (close_window), lc_app);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_window_set_child (GTK_WINDOW (lc_app->window), box);

	stack_sb=gtk_stack_sidebar_new();
	gtk_box_append(GTK_BOX(box), stack_sb);

	stack=gtk_stack_new();
	gtk_box_append(GTK_BOX(box), stack);
	gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(stack_sb), GTK_STACK(stack));

	//
	// Battery page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "Battery", "Battery");

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	gtk_widget_set_margin_top(c, 3);
	w = gtk_label_new("SOC");
	gtk_widget_set_vexpand(w, false);
	gtk_widget_set_hexpand(w, false);
	gtk_box_append(GTK_BOX(c), w);
	lc_app->bat_soc_pbar = gtk_progress_bar_new();
	gtk_widget_set_hexpand(lc_app->bat_soc_pbar, true);
	gtk_widget_set_margin_end(lc_app->bat_soc_pbar, 3);
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(lc_app->bat_soc_pbar), true);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(lc_app->bat_soc_pbar), lc_app->bat_soc / 100.);
	gtk_box_append(GTK_BOX(c), lc_app->bat_soc_pbar);

	w = gtk_frame_new("Start Charge Threshold");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_image_new_from_icon_name("battery-low-charging");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->bat_start_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_start_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->bat_start_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_start_slider), lc_app->bat_start_thres);
    g_signal_connect (lc_app->bat_start_slider, "value-changed", G_CALLBACK (bat_start_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->bat_start_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->bat_start_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->bat_start_slider);
	w = gtk_label_new("%");
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("End Charge Threshold");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_image_new_from_icon_name("battery-full-charged");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
	gtk_box_append(GTK_BOX(c), w);
    lc_app->bat_end_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->bat_end_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->bat_end_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->bat_end_slider), lc_app->bat_end_thres);
    g_signal_connect (lc_app->bat_end_slider, "value-changed", G_CALLBACK (bat_end_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->bat_end_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->bat_end_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->bat_end_slider);
	w = gtk_label_new("%");
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);
	lc_app->bat_undo_btn = gtk_button_new_from_icon_name("edit-undo-symbolic");
	gtk_widget_set_sensitive(lc_app->bat_undo_btn, false);
    g_signal_connect (lc_app->bat_undo_btn, "clicked", G_CALLBACK (bat_thres_undo_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->bat_undo_btn);
	lc_app->bat_apply_btn = gtk_button_new_from_icon_name("emblem-ok-symbolic");
	gtk_widget_set_sensitive(lc_app->bat_apply_btn, false);
	gtk_widget_set_margin_end(lc_app->bat_apply_btn, 3);
    g_signal_connect (lc_app->bat_apply_btn, "clicked", G_CALLBACK (bat_thres_apply_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->bat_apply_btn);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_button_new_with_label("Start charge now!");
	gtk_widget_set_tooltip_text(w, "Will start charging immediately,\nup to End Charge Threshold");
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(w, false);
	}
    g_signal_connect (w, "clicked", G_CALLBACK (start_charge_now_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), w);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_button_new_with_label("Stop charge now!");
	gtk_widget_set_margin_bottom(w, 3);
	gtk_widget_set_tooltip_text(w, "Will stop charging immediately.");
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(w, false);
	}
    g_signal_connect (w, "clicked", G_CALLBACK (stop_charge_now_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), w);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);

	//
	// CPU page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "CPU", "CPU");

	w = gtk_frame_new("Long Term");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	gtk_widget_set_margin_top(w, 3);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_label_new("PL1");
	gtk_box_append(GTK_BOX(c), w);
    lc_app->cpu_pl1_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 20., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->cpu_pl1_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->cpu_pl1_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl1_slider), lc_app->cpu_pl1);
    g_signal_connect (lc_app->cpu_pl1_slider, "value-changed", G_CALLBACK (cpu_pl1_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->cpu_pl1_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->cpu_pl1_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->cpu_pl1_slider);
	w = gtk_label_new("W");
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("Short Term");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_label_new("PL2");
	gtk_box_append(GTK_BOX(c), w);
    lc_app->cpu_pl2_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 35., 1);
    gtk_scale_set_draw_value (GTK_SCALE(lc_app->cpu_pl2_slider), true);
    gtk_scale_set_value_pos(GTK_SCALE(lc_app->cpu_pl2_slider), GTK_POS_RIGHT);
    gtk_range_set_value(GTK_RANGE(lc_app->cpu_pl2_slider), lc_app->cpu_pl2);
    g_signal_connect (lc_app->cpu_pl2_slider, "value-changed", G_CALLBACK (cpu_pl2_val_chg), lc_app);
    gtk_widget_set_hexpand(lc_app->cpu_pl2_slider, true);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(lc_app->cpu_pl2_slider, false);
	}
	gtk_box_append(GTK_BOX(c), lc_app->cpu_pl2_slider);
	w = gtk_label_new("W");
	gtk_box_append(GTK_BOX(c), w);

	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_append(GTK_BOX(box), c);
	w = gtk_label_new("");
    gtk_widget_set_hexpand(w, true);
	gtk_box_append(GTK_BOX(c), w);
	lc_app->cpu_undo_btn = gtk_button_new_from_icon_name("edit-undo-symbolic");
	gtk_widget_set_sensitive(lc_app->cpu_undo_btn, false);
    g_signal_connect (lc_app->cpu_undo_btn, "clicked", G_CALLBACK (cpu_undo_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->cpu_undo_btn);
	lc_app->cpu_apply_btn = gtk_button_new_from_icon_name("emblem-ok-symbolic");
	gtk_widget_set_sensitive(lc_app->cpu_apply_btn, false);
    g_signal_connect (lc_app->cpu_apply_btn, "clicked", G_CALLBACK (cpu_apply_clicked), lc_app);
	gtk_box_append(GTK_BOX(c), lc_app->cpu_apply_btn);

	//
	// LEDs page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "LEDs", "LEDs");

	w = gtk_frame_new("Keyboard Backlight");
	gtk_widget_set_margin_end(w, 3);
	gtk_widget_set_margin_top(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);
	w = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 255., 1);
    gtk_scale_set_draw_value (GTK_SCALE(w), false);
    gtk_widget_set_hexpand(w, true);
    gtk_range_set_value(GTK_RANGE(w), lc_app->kbd_backl);
    g_signal_connect (w, "value-changed", G_CALLBACK (kbd_backl_val_chg), lc_app);
    if (!lc_app->is_root) {
        gtk_widget_set_sensitive(w, false);
    }
	gtk_box_append(GTK_BOX(c), w);

	w = gtk_frame_new("WiFi / BT");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);

	lc_app->rfkill_tbtn1=gtk_check_button_new_with_label("Rfkill");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(lc_app->rfkill_tbtn1),NULL);
	gtk_box_append(GTK_BOX(c), lc_app->rfkill_tbtn1);
	lc_app->rfkill_tbtn2=gtk_check_button_new_with_label("WiFi RX activity");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(lc_app->rfkill_tbtn2),GTK_CHECK_BUTTON(lc_app->rfkill_tbtn1));
	gtk_box_append(GTK_BOX(c), lc_app->rfkill_tbtn2);
	lc_app->rfkill_tbtn3=gtk_check_button_new_with_label("WiFi TX activity");
	gtk_check_button_set_group(GTK_CHECK_BUTTON(lc_app->rfkill_tbtn3),GTK_CHECK_BUTTON(lc_app->rfkill_tbtn1));
	gtk_box_append(GTK_BOX(c), lc_app->rfkill_tbtn3);
	gtk_check_button_set_active(GTK_CHECK_BUTTON(lc_app->rfkill_tbtn1), true);

    g_signal_connect (lc_app->rfkill_tbtn1, "toggled", G_CALLBACK (led_rfkill_toggled), lc_app);
    g_signal_connect (lc_app->rfkill_tbtn2, "toggled", G_CALLBACK (led_rfkill_toggled), lc_app);
    g_signal_connect (lc_app->rfkill_tbtn3, "toggled", G_CALLBACK (led_rfkill_toggled), lc_app);

	w = gtk_frame_new("Notification");
	gtk_widget_set_margin_end(w, 3);
	gtk_box_append(GTK_BOX(box), w);
	c = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_frame_set_child(GTK_FRAME(w), c);

	//
	// Info page
	//
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_stack_add_titled(GTK_STACK(stack), box, "Info", "Info");

	w = gtk_frame_new("DMI");
	gtk_widget_set_margin_end(w, 3);
	gtk_widget_set_margin_top(w, 3);
	gtk_box_append(GTK_BOX(box), w);
    c = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(c), 1);
	gtk_frame_set_child(GTK_FRAME(w), c);
	{
		char buf[128];

		w = gtk_label_new("Product name: ");
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 1, 1, 1, 1);
		get_string_from_text_file(BIOS_DMI_PATH BIOS_DMI_PRODUCT_NAME, buf, 128);
		w = gtk_label_new(buf);
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 2, 1, 1, 1);

	    if (lc_app->is_root) {
			w = gtk_label_new("Serial #: ");
			gtk_widget_set_halign(w, GTK_ALIGN_START);
		    gtk_grid_attach (GTK_GRID(c), w, 1, 2, 1, 1);
			get_string_from_text_file(BIOS_DMI_PATH BIOS_DMI_BOARD_SERIAL, buf, 128);
			w = gtk_label_new(buf);
			gtk_widget_set_halign(w, GTK_ALIGN_START);
		    gtk_grid_attach (GTK_GRID(c), w, 2, 2, 1, 1);
		}

		w = gtk_label_new("BIOS Version: ");
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 1, 3, 1, 1);
		get_string_from_text_file(BIOS_DMI_PATH BIOS_DMI_BIOS_VERSION, buf, 128);
		w = gtk_label_new(buf);
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 2, 3, 1, 1);

		w = gtk_label_new("BIOS Date: ");
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 1, 4, 1, 1);
		get_string_from_text_file(BIOS_DMI_PATH BIOS_DMI_BIOS_DATE, buf, 128);
		w = gtk_label_new(buf);
		gtk_widget_set_halign(w, GTK_ALIGN_START);
	    gtk_grid_attach (GTK_GRID(c), w, 2, 4, 1, 1);
	}

	if (lc_app->is_root) {
		w = gtk_frame_new("EC");
		gtk_widget_set_margin_end(w, 3);
		gtk_box_append(GTK_BOX(box), w);
	    c = gtk_grid_new();
	    gtk_grid_set_row_spacing(GTK_GRID(c), 1);
		gtk_frame_set_child(GTK_FRAME(w), c);
		{
			char buf[0x100];
			int fd;

			fd=port_open();
			if (fd > 0) {
				get_ec_version(fd, buf);
				w = gtk_label_new("Version: ");
				gtk_widget_set_halign(w, GTK_ALIGN_START);
			    gtk_grid_attach (GTK_GRID(c), w, 1, 1, 1, 1);
				w = gtk_label_new(buf);
				gtk_widget_set_halign(w, GTK_ALIGN_START);
			    gtk_grid_attach (GTK_GRID(c), w, 2, 1, 1, 1);

				get_ec_board(fd, buf);
				w = gtk_label_new("Board: ");
				gtk_widget_set_halign(w, GTK_ALIGN_START);
			    gtk_grid_attach (GTK_GRID(c), w, 1, 2, 1, 1);
				w = gtk_label_new(buf);
				gtk_widget_set_halign(w, GTK_ALIGN_START);
			    gtk_grid_attach (GTK_GRID(c), w, 2, 2, 1, 1);
			}
		}
	}
}


void gtest_app_activate (GApplication *application, gpointer user_data)
{
	lcontrol_app_t *lc_app=(lcontrol_app_t *) user_data;

    if (getuid() == 0 || geteuid() == 0) {
        lc_app->is_root=true;
	}

	lc_app->window = gtk_application_window_new (GTK_APPLICATION (application));
    create_main_window(lc_app);
    gtk_window_present (GTK_WINDOW(lc_app->window));
    g_timeout_add_seconds(5, update_values_timer, lc_app);
}

int main (int argc, char **argv)
{
static lcontrol_app_t lcontrol_app;

	lcontrol_app.is_root = false;
	lcontrol_app.cpu_pl1 = 15.0;
	lcontrol_app.cpu_pl1 = 20.0;
	lcontrol_app.bat_soc = 0.;
	lcontrol_app.bat_start_thres = 90;
	lcontrol_app.bat_end_thres = 100;

	update_values_get(&lcontrol_app);

    lcontrol_app.gapp=gtk_application_new("com.purism.librem-control", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(lcontrol_app.gapp, "activate", G_CALLBACK (gtest_app_activate), &lcontrol_app);
    g_application_run (G_APPLICATION (lcontrol_app.gapp), argc, argv);
    g_object_unref (lcontrol_app.gapp);

return 0;
}
