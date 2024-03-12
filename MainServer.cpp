#include <gtk/gtk.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <mutex>

using namespace std;

atomic<bool> server_running(false);
GtkWidget *text_view;
mutex gui_mutex;

// Función para actualizar el TextView en la GUI
void update_text_view(const string &message) {
    gui_mutex.lock();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message.c_str(), -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
    gui_mutex.unlock();
}

// Función para manejar la lógica del servidor en un hilo separado
void server_thread() {
    int listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == -1) {
        cerr << "Can't create a socket!" << endl;
        return;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54000);
    inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == -1) {
        cerr << "Can't bind to IP/port" << endl;
        return;
    }

    if (listen(listening, SOMAXCONN) == -1) {
        cerr << "Can't listen!" << endl;
        return;
    }

    // Espera por conexiones cuando el servidor está corriendo
    while (server_running) {
        sockaddr_in client;
        socklen_t clientSize = sizeof(client);
        int clientSocket = accept(listening, (sockaddr*)&client, &clientSize);
        if (clientSocket == -1) {
            continue; // Si accept falla, intenta de nuevo
        }

        char host[NI_MAXHOST] = {0};
        char service[NI_MAXSERV] = {0};

        if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
            string connection_message = string(host) + " connected on port " + string(service);
            update_text_view(connection_message);
        } else {
            inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
            string connection_message = string(host) + " connected on port " + to_string(ntohs(client.sin_port));
            update_text_view(connection_message);
        }

        // Loop de manejo del cliente
        char buf[4096];
        while (server_running) {
            memset(buf, 0, 4096);

            int bytesReceived = recv(clientSocket, buf, 4096, 0);
            if (bytesReceived <= 0) {
                break;
            }

            string received_message = string(buf, 0, bytesReceived);
            update_text_view("Received: " + received_message);

            // Echo message back to client
            send(clientSocket, buf, bytesReceived, 0);
        }

        close(clientSocket);
    }

    // Cierre de socket de escucha
    close(listening);
}

// Callbacks para botones
static void start_clicked(GtkWidget *widget, gpointer data) {
    if (!server_running) {
        server_running = true;
        std::thread(server_thread).detach(); // Inicia el servidor en un nuevo hilo
        g_print("Server started.\n");
    }
}

static void stop_clicked(GtkWidget *widget, gpointer data) {
    if (server_running) {
        server_running = false; // Esto hará que el servidor detenga su loop y cierre
        g_print("Server stopping...\n");
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window, *button_start, *button_stop, *vbox, *scrolled_window;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Servidor de Eco");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    button_start = gtk_button_new_with_label("Iniciar Servidor");
    g_signal_connect(button_start, "clicked", G_CALLBACK(start_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button_start, TRUE, TRUE, 0);

    button_stop = gtk_button_new_with_label("Detener Servidor");
    g_signal_connect(button_stop, "clicked", G_CALLBACK(stop_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button_stop, TRUE, TRUE, 0);

    // Crear un GtkTextView dentro de un GtkScrolledWindow para mostrar mensajes
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
//telnet localhost 54000
