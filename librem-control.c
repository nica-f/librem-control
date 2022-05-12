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

#include <adwaita.h>

#define LED_RED_PATH			"/sys/class/leds/red:status"
#define LED_GREEN_PATH			"/sys/class/leds/green:status"
#define LED_BLUE_PATH			"/sys/class/leds/blue:status"
#define LED_AIRPLANE_PATH		"/sys/class/leds/librem_ec:airplane"
#define LED_KBD_BACKLIGHT		"/sys/class/leds/librem_ec:kbd_backlight"

#define BAT_START_THRESHOLD_PATH	"/sys/class/power_supply/BAT0/charge_control_start_threshold"
#define BAT_END_THRESHOLD_PATH		"/sys/class/power_supply/BAT0/charge_control_end_threshold"

#define CPU_PL1_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_0_power_limit_uw"
#define CPU_PL2_PATH			"/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/constraint_1_power_limit_uw"

typedef struct {
	GtkWidget *window;
	GtkApplication *gapp;
	GtkWidget *stack;
	GtkWidget *stack_sb;
} lcontrol_app_t ;

static lcontrol_app_t lcontrol_app;

static GtkWidget *bat_start_slider;
static GtkWidget *bat_end_slider;

static gboolean is_root = false;

static void print_me (GtkWidget *widget, gpointer data)
{
    g_print ("UID=%d\n", getuid());
    g_print ("EUID=%d\n", geteuid());
}

void bat_start_val_chg (GtkRange* self, gpointer user_data)
{
    double bat_start_val;
    double bat_end_val;

    bat_end_val = gtk_range_get_value(GTK_RANGE(bat_end_slider));
    bat_start_val = gtk_range_get_value(self);

    if (bat_start_val < 10) {
        bat_start_val = 10;
        gtk_range_set_value(GTK_RANGE(bat_start_slider), bat_start_val);
    }
    if (bat_start_val > 99) {
        bat_start_val = 99;
        gtk_range_set_value(GTK_RANGE(bat_start_slider), bat_start_val);
    }
    bat_start_val+=1;
    if (bat_start_val > bat_end_val) {
        gtk_range_set_value(GTK_RANGE(bat_end_slider), bat_start_val);
    }
}

void bat_end_val_chg (GtkRange* self, gpointer user_data)
{
    double bat_start_val;
    double bat_end_val;

    bat_start_val = gtk_range_get_value(GTK_RANGE(bat_start_slider));
    bat_end_val = gtk_range_get_value(self);

    bat_end_val-=1;
    if (bat_end_val < bat_start_val) {
        gtk_range_set_value(GTK_RANGE(bat_start_slider), bat_end_val);
    }
}

static void close_window (void)
{
    // clean up and quit
    // g_application_quit(gapp);
    gtk_application_remove_window(lcontrol_app.gapp, GTK_WINDOW(lcontrol_app.window));
}

void create_main_window (GtkWidget *appwindow)
{
    GtkWidget *box;
    GtkWidget *w, *w2;

    lcontrol_app.window = appwindow;

    gtk_window_set_title (GTK_WINDOW (lcontrol_app.window), "GApp Test");

    g_signal_connect (lcontrol_app.window, "destroy",
        G_CALLBACK (close_window), NULL);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_window_set_child (GTK_WINDOW (lcontrol_app.window), box);

	lcontrol_app.stack_sb=gtk_stack_sidebar_new();
	gtk_box_append(GTK_BOX(box), lcontrol_app.stack_sb);

	lcontrol_app.stack=gtk_stack_new();
	gtk_box_append(GTK_BOX(box), lcontrol_app.stack);

	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(lcontrol_app.stack_sb), GTK_STACK(lcontrol_app.stack));

    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);

	gtk_stack_add_titled(GTK_STACK(lcontrol_app.stack), box, NULL, "Battery");

#if 0
    w = gtk_label_new("BlaBla");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 1, 1);
    w = gtk_button_new_with_label("PrintMe");
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 2, 1, 1, 1);

    w=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach (GTK_GRID(box), w, 1, 2, 2, 1);
#endif

    w=gtk_image_new_from_icon_name("battery-low-charging");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 1, 2);

    w=gtk_image_new_from_icon_name("battery-full-charged");
    gtk_image_set_icon_size(GTK_IMAGE(w), GTK_ICON_SIZE_LARGE);
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 1, 2);

    w = gtk_label_new("Start charge threshold");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 2, 1);

    bat_start_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(bat_start_slider), true);
    gtk_range_set_value(GTK_RANGE(bat_start_slider), 40.);
    g_signal_connect (bat_start_slider, "value-changed", G_CALLBACK (bat_start_val_chg), NULL);
    gtk_widget_set_hexpand(bat_start_slider, true);
    if (!is_root)
        gtk_widget_set_sensitive(bat_start_slider, false);
    gtk_grid_attach (GTK_GRID(box), bat_start_slider, 2, 2, 1, 1);

    w = gtk_label_new("End charge threshold");
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 2, 1);

    bat_end_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0., 100., 1);
    gtk_scale_set_draw_value (GTK_SCALE(bat_end_slider), true);
    gtk_range_set_value(GTK_RANGE(bat_end_slider), 90.);
    g_signal_connect (bat_end_slider, "value-changed", G_CALLBACK (bat_end_val_chg), NULL);
    if (!is_root)
        gtk_widget_set_sensitive(bat_end_slider, false);
    gtk_grid_attach (GTK_GRID(box), bat_end_slider, 2, 4, 1, 1);

    w = gtk_button_new_with_label("Charge now!");
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 1, 5, 1, 1);

    w = gtk_button_new_with_label("Stop charge");
    gtk_widget_set_hexpand(w, false);
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 2, 5, 1, 1);



    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);
	gtk_stack_add_titled(GTK_STACK(lcontrol_app.stack), box, NULL, "CPU");
    w = gtk_label_new("Long term energy limit (PL1)");
    gtk_grid_attach (GTK_GRID(box), w, 1, 1, 2, 1);
    w = gtk_spin_button_new_with_range(5, 40, .1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 15);
    gtk_grid_attach (GTK_GRID(box), w, 1, 2, 1, 1);
    w = gtk_label_new("W");
    gtk_grid_attach (GTK_GRID(box), w, 2, 2, 1, 1);
    w = gtk_label_new("Short term energy limit (PL2)");
    gtk_grid_attach (GTK_GRID(box), w, 1, 3, 2, 1);
    w = gtk_spin_button_new_with_range(5, 40, .1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 20);
    gtk_grid_attach (GTK_GRID(box), w, 1, 4, 1, 1);
    w = gtk_label_new("W");
    gtk_grid_attach (GTK_GRID(box), w, 2, 4, 1, 1);
    w = gtk_button_new_with_label("Apply");
    g_signal_connect (w, "clicked", G_CALLBACK (print_me), NULL);
    gtk_grid_attach (GTK_GRID(box), w, 2, 5, 1, 1);


    box = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(box), 1);

	gtk_stack_add_titled(GTK_STACK(lcontrol_app.stack), box, NULL, "LEDs");


#if 0
    w = gtk_label_new("WiFi/BT LED");
    gtk_grid_attach (GTK_GRID(box), w, 1, 5, 2, 1);

    w = gtk_radio_button_new_with_label("rfkill");
    w2 = gtk_radio_button_new_with_label("WiFi activity");
    gtk_radio_button_join_group(GTK_RADIO_BUTTON(w));

    gtk_grid_attach (GTK_GRID(box), w, 2, 8, 1, 1);
    gtk_grid_attach (GTK_GRID(box), w2, 2, 9, 1, 1);
#endif
}


void gtest_app_activate (GApplication *application, gpointer user_data)
{
    if (getuid() == 0 || geteuid() == 0) {
        is_root=true;
	}

	lcontrol_app.window = gtk_application_window_new (GTK_APPLICATION (application));
    create_main_window(lcontrol_app.window);
    gtk_window_present (GTK_WINDOW(lcontrol_app.window));
}

int main (int argc, char **argv)
{
    lcontrol_app.gapp=gtk_application_new("com.purism.librem-control", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(lcontrol_app.gapp, "activate", G_CALLBACK (gtest_app_activate), NULL);
    g_application_run (G_APPLICATION (lcontrol_app.gapp), argc, argv);
    g_object_unref (lcontrol_app.gapp);

return 0;
}
