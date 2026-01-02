use libadwaita::ActionRow;

pub struct KPlayer {
    is_playing: bool,
    track_list: Vec<String>,
    actions: Vec<ActionRow>,
}

impl KPlayer {
    pub fn new() -> Self {
        Self {
            is_playing: false,
            track_list: Vec::new(),
            actions: Vec::new(),
        }
    }

    pub fn is_playing(&self) -> bool {
        self.is_playing
    }

    pub fn play(&mut self) {
        self.is_playing = true;
    }

    pub fn pause(&mut self) {
        self.is_playing = false;
    }

    pub fn stop(&mut self) {
        self.pause();
    }
}
