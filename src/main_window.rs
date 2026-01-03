use gettextrs::gettext;
use gtk4::FileDialog;
use gtk4::FileFilter;
use gtk4::SpinButton;
use gtk4::gio::*;
use gtk4::glib;
use gtk4::prelude::*;
use libadwaita::ActionRow;
use libadwaita::HeaderBar;
use libadwaita::StatusPage;
use libadwaita::ToolbarView;
use libadwaita::prelude::ActionRowExt;
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
        let player = Rc::new(RefCell::new(KPlayer::new()));

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
        kalo_menu.append(Some(&gettext("AddSong")), Some("win.add_song"));
        kalo_menu.append(Some(&gettext("Exit")), Some("win.exit_app"));

        let exit_action = ActionEntry::builder("exit_app")
            .activate(|window: &ApplicationWindow, _, _| {
                println!("Exit action triggered");
                window.close();
            })
            .build();

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

        let adjustment = gtk4::Adjustment::new(0.0, 0.0, 100.0, 1.0, 1.0, 0.0);

        let slider_pos = gtk4::Scale::builder()
            .orientation(gtk4::Orientation::Horizontal)
            .css_classes(["fine-tune"])
            .hexpand(true)
            .adjustment(&adjustment)
            .build();

        let label_time = gtk4::Label::new(Some("00:00:00 / 00:00:00"));
        label_time.set_margin_end(5);

        let spin_volume = SpinButton::builder()
            .orientation(gtk4::Orientation::Horizontal)
            .climb_rate(1.0)
            .digits(0)
            .build();

        spin_volume.set_numeric(true);
        spin_volume.set_range(0.0, 150.0);
        spin_volume.set_snap_to_ticks(true);

        play_bar.append(&button_previous);
        play_bar.append(&button_play);
        play_bar.append(&button_next);
        play_bar.append(&slider_pos);
        play_bar.append(&label_time);
        play_bar.append(&spin_volume);

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

        let scroll_menu = gtk4::ScrolledWindow::builder()
            .child(&list_songs)
            .vexpand(true)
            .hscrollbar_policy(gtk4::PolicyType::Never)
            .vscrollbar_policy(gtk4::PolicyType::Automatic)
            .build();

        main_layout.append(&scroll_menu);
        main_layout.append(&play_bar);

        stack.add_named(&main_layout, Some("list-songs"));
        stack.add_named(&status_playlist_empty, Some("playlist-empty"));

        stack.set_visible_child_name("playlist-empty");

        player_box.append(&stack);

        let add_song_action = gtk4::gio::SimpleAction::new("add_song", None);

        add_song_action.connect_activate(move |_, _| {
            println!("Add song action triggered");
            let filter = FileFilter::new();
            filter.set_name(Some(&"Audio"));
            filter.add_mime_type("audio/*");

            let filters = gtk4::gio::ListStore::new::<FileFilter>();
            filters.append(&filter);

            let dialog = FileDialog::builder()
                .title(&gettext("AddSong"))
                .filters(&filters)
                .build();

            dialog.open(
                Some(&window),
                gtk4::gio::Cancellable::NONE,
                glib::clone!(
                    #[weak]
                    stack,
                    #[weak]
                    player,
                    #[weak]
                    list_songs,
                    move |result| {
                        match result {
                            Ok(file) => {
                                let file_pathbuf = file.path().unwrap();

                                let mut audio_player = player.borrow_mut();

                                let action_row = audio_player.load_track(&file_pathbuf);

                                stack.set_visible_child_name("list-songs");

                                if let Some(action) = action_row {
                                    list_songs.append(action);

                                    let tracks = audio_player.tracks();

                                    audio_player.select(tracks - 1);
                                }
                            }
                            Err(err) => {}
                        }
                    }
                ),
            );
        });

        window.add_action(&add_song_action);

        window.add_action_entries([exit_action]);

        list_songs.connect_row_selected(glib::clone!(
            #[weak]
            player,
            move |_, row| {
                let mut player = player.borrow_mut();

                if let Some(row) = row {
                    let row_action = row.downcast_ref::<ActionRow>().unwrap();
                    player.select(row_action.index());
                }
            }
        ));

        button_play.connect_clicked(glib::clone!(
            #[weak]
            player,
            move |button| {
                let mut player = player.borrow_mut();
                if !player.is_playing() {
                    button.set_icon_name("media-playback-pause");
                    player.play();
                } else {
                    button.set_icon_name("media-playback-start");
                    player.pause();
                }
            }
        ));

        toolbar.set_content(Some(&player_box));

        toolbar.add_top_bar(&header);
        window.set_content(Some(&toolbar));

        window.present();
    }

    pub fn run(&self) -> glib::ExitCode {
        let application = Application::builder().application_id(&self.app_id).build();

        application.connect_activate(move |app| Self::build_ui(app));

        application.set_accels_for_action("win.exit_app", &["<Control>x"]);
        application.set_accels_for_action("win.add_song", &["<Control>o"]);

        application.run()
    }
}
