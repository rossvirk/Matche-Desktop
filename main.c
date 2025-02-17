#include <stdio.h>
#include <gtk/gtk.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <glib.h>
#include <glib/gstdio.h>

enum {
    COLUMN_NAME,
    COLUMN_SIZE,
    COLUMN_CREATION_DATE,
    COLUMN_OWNER,
    COLUMN_PERMISSIONS,
    COLUMN_EXECUTABLE,
    COLUMN_PATH,
    NUM_COLUMNS
};

gboolean show_hidden_files = FALSE;
gchar *last_folder_name = NULL;
gint sort_column_id = -1;
GtkSortType sort_type = GTK_SORT_ASCENDING;
GList *directory_stack = NULL;
GtkWidget *back_button;
GtkWidget *forward_button;
GtkWidget *current_path_label;
GList *forward_stack = NULL;
GList *visited_directories = NULL;

gboolean directory_exists(const char *dir_name) {
    struct stat file_stat;
    return (stat(dir_name, &file_stat) == 0 && S_ISDIR(file_stat.st_mode));
}

gchar* get_user_home_directory() {
    struct passwd *pw = getpwuid(getuid());
    return g_strdup(pw->pw_dir);
}

void add_file_info(GtkListStore *store, const char *dir_name, const char *file_name);
void list_directory(const char *dir_name, GtkListStore *store);
void update_directory_list(GtkListStore *store, const char *dir_name);
void on_folder_selected(GtkFileChooserButton *button, gpointer user_data);
void on_delete_button_clicked(GtkButton *button, gpointer user_data);
void on_edit_button_clicked(GtkButton *button, gpointer user_data);
void on_show_hidden_files_toggled(GtkCheckMenuItem *menuitem, gpointer user_data);
void on_sort_by_size_toggled(GtkCheckMenuItem *menuitem, gpointer user_data);
void on_about_activate(GtkMenuItem *menuitem, gpointer user_data);
void refresh_directory(GtkListStore *store);
void on_create_directory_button_clicked(GtkButton *button, gpointer user_data);
void on_tree_view_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void on_back_button_clicked(GtkButton *button, gpointer user_data);
gint sort_files(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data);

void on_extract_button_clicked(GtkButton *button, gpointer user_data);
void on_checkbox_toggled(GtkToggleButton *toggle_button, gpointer user_data);
void extract_zip(const char *archive_path, const char *destination, GtkProgressBar *progress_bar);
void extract_tar(const char *archive_path, const char *destination, GtkProgressBar *progress_bar);
void update_progress_bar(GtkProgressBar *progress_bar, gdouble fraction);
void show_format_not_supported_dialog(GtkWindow *parent_window, const gchar *file_path);
void on_create_file_button_clicked(GtkButton *button, gpointer user_data);
void search_file(GtkWidget *widget, gpointer data);
void on_view_content_button_clicked(GtkButton *button, gpointer user_data);

void on_view_content_edit_button_clicked(GtkButton *button, gpointer user_data);
void on_view_content_done_button_clicked(GtkButton *button, gpointer user_data);

void on_cut_button_clicked(GtkButton *button, gpointer user_data);

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Matche");
    gtk_window_set_default_size(GTK_WINDOW(window), 1035, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *menubar = gtk_menu_bar_new();

    GtkWidget *preferences_menu = gtk_menu_new();
    GtkWidget *preferences_item = gtk_menu_item_new_with_label("⚙️ Preferencias");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(preferences_item), preferences_menu);

    GtkWidget *show_hidden_files_item = gtk_check_menu_item_new_with_label("Mostrar ficheiros ocultos");

    gtk_menu_shell_append(GTK_MENU_SHELL(preferences_menu), show_hidden_files_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), preferences_item);

    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_label("🛟 Axuda");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);

    GtkWidget *about_item = gtk_menu_item_new_with_label("⭐ Sobre");
    g_signal_connect(about_item, "activate", G_CALLBACK(on_about_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    GtkWidget *file_chooser = gtk_file_chooser_button_new("Seleccione Cartafol", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start(GTK_BOX(vbox), file_chooser, FALSE, FALSE, 0);

    GtkListStore *store = gtk_list_store_new(NUM_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_INT,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);

    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Nome",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_NAME,
                                                                         NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Elementos Inter.",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_SIZE,
                                                                         NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Data de modificación",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_CREATION_DATE,
                                                                         NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Propietario",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_OWNER,
                                                                         NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Permisos",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_PERMISSIONS,
                                                                         NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view),
                                gtk_tree_view_column_new_with_attributes("Executábel",
                                                                         gtk_cell_renderer_text_new(),
                                                                         "text", COLUMN_EXECUTABLE,
                                                                         NULL));

    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll_window), tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll_window, TRUE, TRUE, 0);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "🔍 Busca o ficheiro que queres introducindo o seu nome...");
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    GtkWidget *hbox_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_main, FALSE, FALSE, 0);

    GtkWidget *hbox_left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_main), hbox_left, TRUE, TRUE, 0);

    GtkWidget *extract_button = gtk_button_new_with_label("📦 Extraer");
    gtk_box_pack_start(GTK_BOX(hbox_left), extract_button, FALSE, FALSE, 0);
    
    GtkWidget *create_file_button = gtk_button_new_with_label("🟢 Crear ficheiro");
    gtk_box_pack_start(GTK_BOX(hbox_left), create_file_button, FALSE, FALSE, 0);

    GtkWidget *delete_button = gtk_button_new_with_label("🛑 Apagar");
    gtk_box_pack_start(GTK_BOX(hbox_left), delete_button, FALSE, FALSE, 0);

    GtkWidget *edit_button = gtk_button_new_with_label("📝 Editar");
    gtk_box_pack_start(GTK_BOX(hbox_left), edit_button, FALSE, FALSE, 0);

    GtkWidget *create_directory_button = gtk_button_new_with_label("📂 Novo directorio");
    gtk_box_pack_start(GTK_BOX(hbox_left), create_directory_button, FALSE, FALSE, 0);
    
    GtkWidget *view_content_button = gtk_button_new_with_label("👁️ Ver contido");
    gtk_box_pack_start(GTK_BOX(hbox_left), view_content_button, FALSE, FALSE, 0);
    
    GtkWidget *cut_button = gtk_button_new_with_label("✂️ Recortar");
    gtk_box_pack_start(GTK_BOX(hbox_left), cut_button, FALSE, FALSE, 0);

    GtkWidget *hbox_right = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox_main), hbox_right, FALSE, FALSE, 0);

    back_button = gtk_button_new_with_label("⬅️ Volver");
    gtk_box_pack_start(GTK_BOX(hbox_right), back_button, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(back_button, FALSE);

    g_signal_connect(file_chooser, "selection-changed", G_CALLBACK(on_folder_selected), store);
    g_signal_connect(entry, "activate", G_CALLBACK(search_file), store);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_button_clicked), tree_view);
    g_signal_connect(edit_button, "clicked", G_CALLBACK(on_edit_button_clicked), tree_view);
    g_signal_connect(extract_button, "clicked", G_CALLBACK(on_extract_button_clicked), tree_view);
    g_signal_connect(show_hidden_files_item, "toggled", G_CALLBACK(on_show_hidden_files_toggled), tree_view);
    g_signal_connect(create_directory_button, "clicked", G_CALLBACK(on_create_directory_button_clicked), store);
    g_signal_connect(create_file_button, "clicked", G_CALLBACK(on_create_file_button_clicked), store);
    g_signal_connect(view_content_button, "clicked", G_CALLBACK(on_view_content_button_clicked), tree_view);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_tree_view_row_activated), store);
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_button_clicked), store);
    g_signal_connect(cut_button, "clicked", G_CALLBACK(on_cut_button_clicked), tree_view);

    gchar *home_directory = get_user_home_directory();
    directory_stack = g_list_prepend(directory_stack, g_strdup(home_directory));
    list_directory(home_directory, store);
    g_free(home_directory);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COLUMN_SIZE, GTK_SORT_DESCENDING);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}


void on_view_content_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *file_path;
        gtk_tree_model_get(model, &iter, COLUMN_PATH, &file_path, -1);

        const gchar *mime_type = g_content_type_guess(file_path, NULL, 0, NULL);

        GtkWidget *dialog = gtk_dialog_new_with_buttons("Visualizando Contido",
                                                        NULL,
                                                        GTK_DIALOG_MODAL,
                                                        "Fechar", GTK_RESPONSE_CLOSE,
                                                        NULL);

        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        if (g_str_has_prefix(mime_type, "image/")) {
            GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(file_path, NULL);
            if (pixbuf) {
                GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_box_pack_start(GTK_BOX(content_area), image, TRUE, TRUE, 0);
            } else {
                GtkWidget *error_label = gtk_label_new("Erro ao carregar a imagem.");
                gtk_box_pack_start(GTK_BOX(content_area), error_label, TRUE, TRUE, 0);
            }
        } else {
            FILE *fp = fopen(file_path, "r");
            if (fp == NULL) {
                gtk_widget_destroy(dialog);
                g_free(file_path);
                return;
            }

            GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
            GtkWidget *text_view = gtk_text_view_new();
            gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);

            gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
            gtk_box_pack_start(GTK_BOX(content_area), scrolled_window, TRUE, TRUE, 0);
            gtk_widget_set_size_request(scrolled_window, 900, 400);

            char buffer[4096];
            GtkTextBuffer *buffer_text = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
            GtkTextIter iter_text;

            gtk_text_buffer_get_end_iter(buffer_text, &iter_text);

            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                if (g_utf8_validate(buffer, -1, NULL)) {
                    gtk_text_buffer_insert(buffer_text, &iter_text, buffer, -1);
                } else {
                    gchar *utf8_text = g_convert(buffer, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
                    if (utf8_text) {
                        gtk_text_buffer_insert(buffer_text, &iter_text, utf8_text, -1);
                        g_free(utf8_text);
                    }
                }
            }

            fclose(fp);
        }

        gtk_widget_show_all(dialog);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_free(file_path);
    }
}

void on_view_content_edit_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTextView *text_view = GTK_TEXT_VIEW(user_data);
    gboolean editable = gtk_text_view_get_editable(text_view);
    gchar *file_path = (gchar *)g_object_get_data(G_OBJECT(button), "file_path");
    GtkButton *done_button = GTK_BUTTON(g_object_get_data(G_OBJECT(button), "done_button"));

    if (editable) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        FILE *fp = fopen(file_path, "w");
        if (fp != NULL) {
            fputs(text, fp);
            fclose(fp);
        }
        g_free(text);

        gtk_text_view_set_editable(text_view, FALSE);
        gtk_button_set_label(button, "Editar");
        gtk_button_set_label(done_button, "Listo");
    } else {
        gtk_text_view_set_editable(text_view, TRUE);
        gtk_button_set_label(button, "Salvar");
        gtk_button_set_label(done_button, "Cancelar");
    }
}

void on_view_content_done_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(user_data);
    GtkTextView *text_view = GTK_TEXT_VIEW(g_object_get_data(G_OBJECT(button), "text_view"));
    GtkButton *edit_button = GTK_BUTTON(g_object_get_data(G_OBJECT(button), "edit_button"));
    
    if (gtk_text_view_get_editable(text_view)) {
        gtk_text_view_set_editable(text_view, FALSE);
        gtk_button_set_label(button, "Listo");
        gtk_button_set_label(edit_button, "Editar");
    } else {
        gtk_widget_destroy(dialog);
    }
}

void search_file(GtkWidget *widget, gpointer data) {
    GtkEntry *entry = GTK_ENTRY(widget);
    const gchar *filename = gtk_entry_get_text(entry);

    gchar *home_directory = get_user_home_directory();
    gchar *command = g_strdup_printf("find %s -name '*%s*'", home_directory, filename);
    
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        g_free(home_directory);
        g_free(command);
        return;
    }

    gchar path[1024];
    gboolean found = FALSE;
    GtkTreeModel *model = GTK_TREE_MODEL(data);
    GtkTreeIter iter;

    while (fgets(path, sizeof(path) - 1, fp) != NULL) {
        found = TRUE;
        path[strcspn(path, "\n")] = '\0';

        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_NONE,
                                                   "Arquivo atopado:\n%s",
                                                   path);
        gtk_window_set_title(GTK_WINDOW(dialog), "Aviso de demanda");
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Ir", GTK_RESPONSE_ACCEPT);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancelar", GTK_RESPONSE_REJECT);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_ACCEPT) {
            gchar *folder_path = g_path_get_dirname(path);
            if (last_folder_name) {
                directory_stack = g_list_prepend(directory_stack, g_strdup(last_folder_name));
            }
            last_folder_name = g_strdup(folder_path);
            gtk_widget_set_sensitive(back_button, TRUE);
            update_directory_list(GTK_LIST_STORE(model), folder_path);
            g_free(folder_path);
        }

        break;
    }

    if (!found) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_WARNING,
                                                   GTK_BUTTONS_OK,
                                                   "Non se atopou o ficheiro");
        gtk_window_set_title(GTK_WINDOW(dialog), "Aviso de demanda");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    pclose(fp);
    g_free(home_directory);
    g_free(command);
}

void add_file_info(GtkListStore *store, const char *dir_name, const char *file_name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir_name, file_name);

    struct stat file_stat;
    if (stat(path, &file_stat) == -1) {
        perror("stat");
        return;
    }

    char creation_date[64];
    strftime(creation_date, sizeof(creation_date), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_ctime));

    struct passwd *pw = getpwuid(file_stat.st_uid);
    char *owner = pw ? pw->pw_name : "unknown";

    char permissions[11];
    snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c%c",
             S_ISDIR(file_stat.st_mode) ? 'd' : '-',
             (file_stat.st_mode & S_IRUSR) ? 'r' : '-',
             (file_stat.st_mode & S_IWUSR) ? 'w' : '-',
             (file_stat.st_mode & S_IXUSR) ? 'x' : '-',
             (file_stat.st_mode & S_IRGRP) ? 'r' : '-',
             (file_stat.st_mode & S_IWGRP) ? 'w' : '-',
             (file_stat.st_mode & S_IXGRP) ? 'x' : '-',
             (file_stat.st_mode & S_IROTH) ? 'r' : '-',
             (file_stat.st_mode & S_IWOTH) ? 'w' : '-',
             (file_stat.st_mode & S_IXOTH) ? 'x' : '-');

    gboolean executable = (file_stat.st_mode & S_IXUSR) ? TRUE : FALSE;

    gint num_items = 0;
    if (S_ISDIR(file_stat.st_mode)) {
        DIR *dir = opendir(path);
        if (dir != NULL) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    num_items++;
                }
            }
            closedir(dir);
        }
    }

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_NAME, file_name,
                       COLUMN_SIZE, num_items,
                       COLUMN_CREATION_DATE, creation_date,
                       COLUMN_OWNER, owner,
                       COLUMN_PERMISSIONS, permissions,
                       COLUMN_EXECUTABLE, executable ? "Si" : "Non",
                       COLUMN_PATH, path,
                       -1);
}

void list_directory(const char *dir_name, GtkListStore *store) {
    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' || show_hidden_files) {
            if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
                add_file_info(store, dir_name, entry->d_name);
            }
        }
    }

    closedir(dir);

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
        gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
        gtk_tree_path_free(path);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COLUMN_SIZE, GTK_SORT_DESCENDING);
}


void on_folder_selected(GtkFileChooserButton *button, gpointer user_data) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(button);
    gchar *folder_name = gtk_file_chooser_get_filename(chooser);

    if (folder_name) {
        GtkListStore *store = user_data;
        gtk_list_store_clear(store);
        list_directory(folder_name, store);

        if (last_folder_name) {
            g_free(last_folder_name);
        }
        last_folder_name = g_strdup(folder_name);

        g_free(folder_name);
    }
}

void update_directory_list(GtkListStore *store, const char *dir_name) {
    gtk_list_store_clear(store);
    list_directory(dir_name, store);
}

void on_tree_view_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *file_name;
        gchar *file_path;
        gtk_tree_model_get(model, &iter, COLUMN_NAME, &file_name, COLUMN_PATH, &file_path, -1);

        struct stat file_stat;
        if (stat(file_path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
            if (last_folder_name) {
                directory_stack = g_list_prepend(directory_stack, g_strdup(last_folder_name));
            }
            last_folder_name = g_strdup(file_path);
            gtk_widget_set_sensitive(back_button, TRUE);
            update_directory_list(GTK_LIST_STORE(model), file_path);
        }

        g_free(file_name);
        g_free(file_path);
    }
}

void on_delete_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *file_path;
        gtk_tree_model_get(model, &iter, COLUMN_PATH, &file_path, -1);

        struct stat file_stat;
        if (stat(file_path, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                if (rmdir(file_path) == -1) {
                    char command[1024];
                    snprintf(command, sizeof(command), "rm -rf %s", file_path);
                    system(command);
                }
            } else {
                remove(file_path);
            }
        }

        g_free(file_path);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

void on_back_button_clicked(GtkButton *button, gpointer user_data) {
    if (directory_stack != NULL) {
        gchar *previous_folder = directory_stack->data;
        directory_stack = g_list_remove(directory_stack, previous_folder);

        if (last_folder_name) {
            g_free(last_folder_name);
        }
        last_folder_name = g_strdup(previous_folder);

        GtkListStore *store = GTK_LIST_STORE(user_data);
        gtk_list_store_clear(store);
        list_directory(previous_folder, store);

        gtk_widget_set_sensitive(back_button, directory_stack != NULL);

        if (directory_stack == NULL) {
            gchar *home_directory = get_user_home_directory();
            if (g_strcmp0(last_folder_name, home_directory) == 0) {
                gtk_widget_set_sensitive(back_button, FALSE);
            }
            g_free(home_directory);
        }

        g_free(previous_folder);
    }
}

void on_edit_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *file_path;
        gchar *file_name;
        gchar *permissions;
        gchar *executable;
        gtk_tree_model_get(model, &iter, COLUMN_PATH, &file_path, COLUMN_NAME, &file_name, COLUMN_PERMISSIONS, &permissions, COLUMN_EXECUTABLE, &executable, -1);

        GtkWidget *dialog, *content_area, *entry;
        GtkWidget *checkbox_write, *checkbox_read, *checkbox_execute;
        dialog = gtk_dialog_new_with_buttons("Editar o nome do ficheiro",
                                             GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                             GTK_DIALOG_MODAL,
                                             "Confirmar", GTK_RESPONSE_ACCEPT,
                                             "Cancelar", GTK_RESPONSE_REJECT,
                                             NULL);
        content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), file_name);
        gtk_container_add(GTK_CONTAINER(content_area), entry);

        checkbox_write = gtk_check_button_new_with_label("Permitir escrito");
        checkbox_read = gtk_check_button_new_with_label("Permitir lectura");
        checkbox_execute = gtk_check_button_new_with_label("Facer executable");
        gtk_container_add(GTK_CONTAINER(content_area), checkbox_write);
        gtk_container_add(GTK_CONTAINER(content_area), checkbox_read);
        gtk_container_add(GTK_CONTAINER(content_area), checkbox_execute);

        if (permissions[2] == 'w') {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_write), TRUE);
        }
        if (permissions[1] == 'r') {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_read), TRUE);
        }
        if (permissions[3] == 'x') {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_execute), TRUE);
        }

        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
            char new_path[1024];
            snprintf(new_path, sizeof(new_path), "%s/%s", g_path_get_dirname(file_path), new_name);

            if (rename(file_path, new_path) == 0) {
                gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_NAME, new_name, COLUMN_PATH, new_path, -1);

                mode_t mode = 0;
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_write))) {
                    mode |= S_IWUSR;
                }
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_read))) {
                    mode |= S_IRUSR;
                }
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_execute))) {
                    mode |= S_IXUSR;
                }
                if (chmod(new_path, mode) != 0) {
                    g_print("Produciuse un erro ao aplicar os permisos ao ficheiro\n");
                } else {
                    char new_permissions[11];
                    snprintf(new_permissions, sizeof(new_permissions), "%c%c%c%c%c%c%c%c%c%c",
                             S_ISDIR(mode) ? 'd' : '-',
                             (mode & S_IRUSR) ? 'r' : '-',
                             (mode & S_IWUSR) ? 'w' : '-',
                             (mode & S_IXUSR) ? 'x' : '-',
                             (mode & S_IRGRP) ? 'r' : '-',
                             (mode & S_IWGRP) ? 'w' : '-',
                             (mode & S_IXGRP) ? 'x' : '-',
                             (mode & S_IROTH) ? 'r' : '-',
                             (mode & S_IWOTH) ? 'w' : '-',
                             (mode & S_IXOTH) ? 'x' : '-');
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_PERMISSIONS, new_permissions, -1);

                    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_execute))) {
                        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_EXECUTABLE, "Si", -1);
                    } else {
                        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_EXECUTABLE, "Non", -1);
                    }
                }
            } else {
                g_print("Erro ao renomear o ficheiro\n");
            }
        }

        gtk_widget_destroy(dialog);
        g_free(file_path);
        g_free(file_name);
        g_free(permissions);
        g_free(executable);
    }
}

void on_create_directory_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog, *content_area, *entry, *folder_chooser, *dialog_vbox;
    dialog = gtk_dialog_new_with_buttons("Crear directorio",
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                         GTK_DIALOG_MODAL,
                                         "Confirmar", GTK_RESPONSE_ACCEPT,
                                         "Cancelar", GTK_RESPONSE_REJECT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    dialog_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(content_area), dialog_vbox);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nome do directorio");
    gtk_box_pack_start(GTK_BOX(dialog_vbox), entry, FALSE, FALSE, 0);

    folder_chooser = gtk_file_chooser_button_new("Seleccione Localización de creación", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_box_pack_start(GTK_BOX(dialog_vbox), folder_chooser, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *dir_name = gtk_entry_get_text(GTK_ENTRY(entry));
        gchar *base_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(folder_chooser));
        gchar *folder_path = g_strdup_printf("%s/%s", base_path, dir_name);

        if (mkdir(folder_path, 0755) == 0) {
            if (last_folder_name && g_str_has_prefix(folder_path, last_folder_name)) {
                GtkListStore *store = GTK_LIST_STORE(user_data);
                refresh_directory(store);
            }
        } else {
            g_print("Erro ao crear o directorio: %s\n", folder_path);
        }
        g_free(folder_path);
        g_free(base_path);
    }

    gtk_widget_destroy(dialog);
}

void on_create_file_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog, *content_area, *entry;
    dialog = gtk_dialog_new_with_buttons("Crear novo ficheiro",
                                         GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                         GTK_DIALOG_MODAL,
                                         "Confirmar", GTK_RESPONSE_ACCEPT,
                                         "Cancelar", GTK_RESPONSE_REJECT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nome do ficheiro");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *file_name = gtk_entry_get_text(GTK_ENTRY(entry));
        gchar *file_path = g_strdup_printf("%s/%s", last_folder_name, file_name);

        FILE *file = fopen(file_path, "w");
        if (file) {
            fclose(file);
            GtkListStore *store = GTK_LIST_STORE(user_data);
            add_file_info(store, last_folder_name, file_name);
        } else {
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                             GTK_DIALOG_MODAL,
                                                             GTK_MESSAGE_ERROR,
                                                             GTK_BUTTONS_OK,
                                                             "Erro ao crear o ficheiro: \n%s",
                                                             file_path);
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
        }

        g_free(file_path);
    }

    gtk_widget_destroy(dialog);
}

void on_show_hidden_files_toggled(GtkCheckMenuItem *menuitem, gpointer user_data) {
    show_hidden_files = gtk_check_menu_item_get_active(menuitem);
    GtkWidget *tree_view = GTK_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    GtkListStore *store = GTK_LIST_STORE(model);

    gtk_list_store_clear(store);

    if (last_folder_name) {
        list_directory(last_folder_name, store);
    } else {
        gchar *home_directory = get_user_home_directory();
        list_directory(home_directory, store);
        g_free(home_directory);
    }
}

void on_sort_by_size_toggled(GtkCheckMenuItem *menuitem, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    if (gtk_check_menu_item_get_active(menuitem)) {
        sort_column_id = COLUMN_SIZE;
        sort_type = GTK_SORT_DESCENDING;
    } else {
        sort_column_id = COLUMN_SIZE;
        sort_type = GTK_SORT_ASCENDING;
    }

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), sort_column_id, (GtkTreeIterCompareFunc)sort_files, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), sort_column_id, sort_type);

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        gtk_tree_model_row_changed(model, path, &iter);
        gtk_tree_path_free(path);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Sobre Matche",
                                                    NULL,
                                                    GTK_DIALOG_MODAL,
                                                    "Fechar", GTK_RESPONSE_CLOSE,
                                                    NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *label_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label_title), "<b>Matche | Copyright © 2025, Rossvirk</b>");
    gtk_box_pack_start(GTK_BOX(content_area), label_title, FALSE, FALSE, 5);
    
    GtkWidget *label_text = gtk_label_new("Matche é un proxecto inspirado na sinxeleza do entorno MATE, un xestor de ficheiros\nsimplista cunha representación amorosa da lingua galega.");
    gtk_label_set_justify(GTK_LABEL(label_text), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(content_area), label_text, FALSE, FALSE, 5);
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void refresh_directory(GtkListStore *store) {
    if (last_folder_name) {
        gtk_list_store_clear(store);
        list_directory(last_folder_name, store);
    }
}

void extract_zip(const char *archive_path, const char *destination, GtkProgressBar *progress_bar) {
    char command[1024];
    snprintf(command, sizeof(command), "unzip -qq -o %s -d %s", archive_path, destination);

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        return;
    }

    update_progress_bar(progress_bar, 0.5);
    pclose(fp);
    update_progress_bar(progress_bar, 1.0);
}

void extract_tar(const char *archive_path, const char *destination, GtkProgressBar *progress_bar) {
    char command[1024];
    if (g_str_has_suffix(archive_path, ".tar.gz")) {
        snprintf(command, sizeof(command), "tar -xzf %s -C %s", archive_path, destination);
    } else if (g_str_has_suffix(archive_path, ".tar.xz")) {
        snprintf(command, sizeof(command), "tar -xJf %s -C %s", archive_path, destination);
    } else if (g_str_has_suffix(archive_path, ".tar.bz2")) {
        snprintf(command, sizeof(command), "tar -xjf %s -C %s", archive_path, destination);
    }

    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        return;
    }

    update_progress_bar(progress_bar, 0.5);
    pclose(fp);
    update_progress_bar(progress_bar, 1.0);
}

void show_format_not_supported_dialog(GtkWindow *parent_window, const gchar *file_path) {
    GtkWidget *dialog = gtk_message_dialog_new(parent_window,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "Formato de ficheiro non compatible para a extracción: \n%s",
                                               file_path);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_extract_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *file_path;
        gtk_tree_model_get(model, &iter, COLUMN_PATH, &file_path, -1);

        GtkWidget *dialog, *content_area, *checkbox_extract_here, *file_chooser, *progress_bar;
        dialog = gtk_dialog_new_with_buttons("Extraer ficheiro",
                                             GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                             GTK_DIALOG_MODAL,
                                             "Confirmar", GTK_RESPONSE_ACCEPT,
                                             "Cancelar", GTK_RESPONSE_REJECT,
                                             NULL);
        content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        checkbox_extract_here = gtk_check_button_new_with_label("Extraer aquí");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_extract_here), TRUE);
        gtk_container_add(GTK_CONTAINER(content_area), checkbox_extract_here);

        file_chooser = gtk_file_chooser_button_new("Escolla o cartafol de destino", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gtk_widget_set_sensitive(file_chooser, FALSE);
        gtk_container_add(GTK_CONTAINER(content_area), file_chooser);

        progress_bar = gtk_progress_bar_new();
        gtk_container_add(GTK_CONTAINER(content_area), progress_bar);

        g_signal_connect(checkbox_extract_here, "toggled", G_CALLBACK(on_checkbox_toggled), file_chooser);
        gtk_widget_show_all(dialog);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            const char *destination;
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_extract_here))) {
                destination = last_folder_name;
            } else {
                destination = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
            }

            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
            while (gtk_events_pending()) gtk_main_iteration();

            if (g_str_has_suffix(file_path, ".zip")) {
                extract_zip(file_path, destination, GTK_PROGRESS_BAR(progress_bar));
            } else if (g_str_has_suffix(file_path, ".tar.gz") ||
                       g_str_has_suffix(file_path, ".tar.xz") ||
                       g_str_has_suffix(file_path, ".tar.bz2")) {
                extract_tar(file_path, destination, GTK_PROGRESS_BAR(progress_bar));
            } else {

                show_format_not_supported_dialog(GTK_WINDOW(dialog), file_path);
            }

            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
            update_directory_list(GTK_LIST_STORE(model), destination);
        }

        gtk_widget_destroy(dialog);
        g_free(file_path);
    }
}

void on_checkbox_toggled(GtkToggleButton *toggle_button, gpointer user_data) {
    GtkWidget *file_chooser = GTK_WIDGET(user_data);
    gtk_widget_set_sensitive(file_chooser, !gtk_toggle_button_get_active(toggle_button));
}

void update_progress_bar(GtkProgressBar *progress_bar, gdouble fraction) {
    gtk_progress_bar_set_fraction(progress_bar, fraction);
    while (gtk_events_pending()) gtk_main_iteration();
}

void on_cut_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar *file_path;
        gtk_tree_model_get(model, &iter, COLUMN_PATH, &file_path, -1);

        GtkWidget *dialog = gtk_file_chooser_dialog_new("Selecionar Pasta de Destino",
                                                        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                        "Cancelar", GTK_RESPONSE_CANCEL,
                                                        "Mover", GTK_RESPONSE_ACCEPT,
                                                        NULL);

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            gchar *destination_folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            gchar *new_path = g_build_filename(destination_folder, g_path_get_basename(file_path), NULL);

            if (rename(file_path, new_path) == 0) {
                gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
                update_directory_list(GTK_LIST_STORE(model), destination_folder);

                GtkWidget *success_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                                   GTK_DIALOG_MODAL,
                                                                   GTK_MESSAGE_INFO,
                                                                   GTK_BUTTONS_OK,
                                                                   "O ficheiro moveuse correctamente e serás redirixido a: \n%s",
                                                                   new_path);
                gtk_dialog_run(GTK_DIALOG(success_dialog));
                gtk_widget_destroy(success_dialog);
            } else {
                GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                                 GTK_DIALOG_MODAL,
                                                                 GTK_MESSAGE_ERROR,
                                                                 GTK_BUTTONS_OK,
                                                                 "Erro ao mover o ficheiro: \n%s",
                                                                 file_path);
                gtk_dialog_run(GTK_DIALOG(error_dialog));
                gtk_widget_destroy(error_dialog);
            }

            g_free(destination_folder);
            g_free(new_path);
        }

        gtk_widget_destroy(dialog);
        g_free(file_path);
    }
}

gint sort_files(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
    gint result = 0;
    switch (sort_column_id) {
        case COLUMN_NAME: {
            gchar *name_a, *name_b;
            gtk_tree_model_get(model, a, COLUMN_NAME, &name_a, -1);
            gtk_tree_model_get(model, b, COLUMN_NAME, &name_b, -1);
            result = g_strcmp0(name_a, name_b);
            g_free(name_a);
            g_free(name_b);
            break;
        }
        case COLUMN_CREATION_DATE: {
            gchar *date_a, *date_b;
            gtk_tree_model_get(model, a, COLUMN_CREATION_DATE, &date_a, -1);
            gtk_tree_model_get(model, b, COLUMN_CREATION_DATE, &date_b, -1);
            result = g_strcmp0(date_a, date_b);
            g_free(date_a);
            g_free(date_b);
            break;
        }
        case COLUMN_SIZE: {
            gint size_a, size_b;
            gtk_tree_model_get(model, a, COLUMN_SIZE, &size_a, -1);
            gtk_tree_model_get(model, b, COLUMN_SIZE, &size_b, -1);
            result = (size_a > size_b) - (size_a < size_b);
            break;
        }
    }
    return (sort_type == GTK_SORT_ASCENDING) ? result : -result;
}
