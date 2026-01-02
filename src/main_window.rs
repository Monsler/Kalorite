use gettextrs::gettext;
use gtk4::gio::*;
use gtk4::glib;
use gtk4::glib::clone;
use gtk4::prelude::*;
use libadwaita::ActionRow;
use libadwaita::HeaderBar;
use libadwaita::StatusPage;
use libadwaita::ToolbarView;
use libadwaita::prelude::AdwApplicationWindowExt;
use libadwaita::{Application, ApplicationWindow};

use crate::player::KPlayer;
use std::cell::RefCell;
use std::rc::Rc;

pub struct MainWindow {
    app_id: String,
}

impl MainWindow {
    pub fn new(app_id: &str) -> Self {
        Self {
            app_id: app_id.to_string(),
        }
    }

    fn build_ui(application: &Application) {
        let mut player = Rc::new(RefCell::new(KPlayer::new()));

        let window = ApplicationWindow::builder()
            .title("Kalorite")
            .application(application)
            .icon_name("io.github.monsler.Kalorite")
            .default_width(550)
            .default_height(350)
            .build();

        let header = HeaderBar::new();
        let toolbar = ToolbarView::new();
        let app_icon = gtk4::Image::from_icon_name("io.github.monsler.Kalorite");
        app_icon.set_margin_start(10);
        header.pack_start(&app_icon);

        let player_box = gtk4::Box::builder()
            .orientation(gtk4::Orientation::Vertical)
            .spacing(10)
            .build();

        let kalo_menu = Menu::new();
        kalo_menu.append(Some(&gettext("AddSong")), Some("add_song"));
        kalo_menu.append(Some(&gettext("Exit")), Some("win.exit_app"));

        let exit_action = ActionEntry::builder("exit_app")
            .activate(|window: &ApplicationWindow, _, _| {
                window.close();
            })
            .build();

        window.add_action_entries([exit_action]);

        let menu = gtk4::MenuButton::builder()
            .icon_name("open-menu-symbolic")
            .menu_model(&kalo_menu)
            .build();

        header.pack_end(&menu);

        let main_layout = gtk4::Box::builder()
            .orientation(gtk4::Orientation::Vertical)
            .build();

        let play_bar = gtk4::Box::builder()
            .orientation(gtk4::Orientation::Horizontal)
            .margin_start(10)
            .margin_end(10)
            .margin_bottom(10)
            .margin_top(10)
            .spacing(10)
            .build();

        let stack = gtk4::Stack::new();

        let button_previous = gtk4::Button::builder()
            .icon_name("media-skip-backward")
            .build();

        let button_next = gtk4::Button::builder()
            .icon_name("media-skip-forward")
            .build();

        let button_play = gtk4::Button::builder()
            .icon_name("media-playback-start")
            .build();

        button_play.connect_clicked(move |button| {
            if !player.borrow().is_playing() {
                button.set_icon_name("media-playback-pause");
                player.borrow_mut().play();
            } else {
                button.set_icon_name("media-playback-start");
                player.borrow_mut().pause();
            }
        });

        let adjustment = gtk4::Adjustment::new(0.0, 0.0, 100.0, 1.0, 1.0, 0.0);

        let slider_pos = gtk4::Scale::builder()
            .orientation(gtk4::Orientation::Horizontal)
            .css_classes(["fine-tune"])
            .hexpand(true)
            .adjustment(&adjustment)
            .build();

        let label_time = gtk4::Label::new(Some("00:00:00 / 00:00:00"));
        label_time.set_margin_end(5);

        play_bar.append(&button_previous);
        play_bar.append(&button_play);
        play_bar.append(&button_next);
        play_bar.append(&slider_pos);
        play_bar.append(&label_time);

        let status_playlist_empty = StatusPage::builder()
            .icon_name("audio-x-generic-symbolic")
            .title(&gettext("PlaylistIsEmpty"))
            .description(&gettext("AddSomeMusicPrompt"))
            .build();

        let list_songs = gtk4::ListBox::builder()
            .css_classes(["boxed-list"])
            .margin_start(10)
            .margin_end(10)
            .build();

        let spacer = gtk4::Box::builder()
            .vexpand(true)
            .orientation(gtk4::Orientation::Vertical)
            .build();

        let action = ActionRow::builder()
            .title("Kalorite in rust")
            .subtitle("/home/monsler/kalorite/in/rust")
            .build();

        let action_2 = ActionRow::builder()
            .title("Kalorite in rust")
            .subtitle("/home/monsler/kalorite/in/rust")
            .build();

        list_songs.append(&action);
        list_songs.append(&action_2);

        main_layout.append(&list_songs);
        main_layout.append(&spacer);
        main_layout.append(&play_bar);

        stack.add_named(&main_layout, Some("list-songs"));
        stack.add_named(&status_playlist_empty, Some("playlist-empty"));

        stack.set_visible_child_name("list-songs");

        player_box.append(&stack);

        toolbar.set_content(Some(&player_box));

        toolbar.add_top_bar(&header);
        window.set_content(Some(&toolbar));
        window.present();
    }

    pub fn run(&self) -> glib::ExitCode {
        let application = Application::builder().application_id(&self.app_id).build();

        application.connect_activate(move |app| Self::build_ui(app));

        application.run()
    }
}
