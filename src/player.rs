use gtk4::{MediaFile, prelude::MediaStreamExt};
use std::path::PathBuf;

use libadwaita::ActionRow;

pub struct KPlayer {
    is_playing: bool,
    track_list: Vec<String>,
    actions: Vec<ActionRow>,
    mixer: Option<MediaFile>,
}

impl KPlayer {
    pub fn new() -> Self {
        Self {
            is_playing: false,
            track_list: Vec::new(),
            actions: Vec::new(),
            mixer: None,
        }
    }

    pub fn is_playing(&self) -> bool {
        self.is_playing
    }

    pub fn load_track(&mut self, path: &PathBuf) -> Option<&ActionRow> {
        let track_path = path.to_string_lossy().to_string();

        if !self.track_list.contains(&track_path) {
            let action = ActionRow::builder()
                .title(path.file_name().unwrap().to_string_lossy())
                .subtitle(path.to_string_lossy())
                .build();

            self.track_list.push(track_path);

            self.actions.push(action);

            Some(&self.actions.last().unwrap())
        } else {
            None
        }
    }

    pub fn tracks(&self) -> i32 {
        self.track_list.len() as i32
    }

    pub fn select(&mut self, idx: i32) {
        if let Some(track_path) = self.track_list.get(idx as usize) {
            let mixer = MediaFile::for_filename(track_path);
            self.mixer = Some(mixer);
        }
    }

    pub fn play(&mut self) {
        self.is_playing = true;
        if let Some(mixer) = &mut self.mixer {
            mixer.play();
        }
    }

    pub fn pause(&mut self) {
        self.is_playing = false;
        if let Some(mixer) = &mut self.mixer {
            mixer.pause();
        }
    }

    pub fn get_duration(&self) -> Option<i64> {
        if let Some(mixer) = &self.mixer {
            Some(mixer.duration())
        } else {
            None
        }
    }

    pub fn get_playback_position(&self) -> Option<i64> {
        if let Some(mixer) = &self.mixer {
            Some(mixer.timestamp())
        } else {
            None
        }
    }

    pub fn stop(&mut self) {
        self.pause();
        if let Some(mixer) = &mut self.mixer {
            mixer.pause();
            mixer.seek(0);
        }
    }
}
