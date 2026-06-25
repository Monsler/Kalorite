use std::{error::Error, fs::File, time::Duration};

use audio_engine::*;
use rodio::*;

/*
======================================
    Coded by Monsler at 25/06/2026
       Kalorite rodio based audio
               backend
======================================
*/

pub struct RodioBackend {
    state: PlaybackState,
    sink: MixerDeviceSink,
    player: Player,
    current_duration: Option<Duration>
}

impl RodioBackend {
    pub fn new() -> Result<Self, Box<dyn Error + Send + Sync>> {
        let sink = rodio::DeviceSinkBuilder::open_default_sink()?;
        let player = rodio::Player::connect_new(&sink.mixer());

        Ok(Self {
            state: PlaybackState::Stopped,
            sink,
            current_duration: None,
            player
        })
    }
}

impl SoundBackend for RodioBackend {
    fn load_track(&mut self, path: &std::path::Path) -> Result<(), String> {
        self.stop();

        self.sink = rodio::DeviceSinkBuilder::open_default_sink().map_err(|e| format!("Unable to open sink: {e}"))?;
        self.player = rodio::Player::connect_new(&self.sink.mixer());

        let file = File::open(path).map_err(|e| format!("Unable to read file: {e}"))?;
        let source = Decoder::try_from(file).map_err(|e| format!("Unable to decode: {e}"))?;

        self.current_duration = source.total_duration();

        self.sink.mixer().add(source);

        Ok(())
    }

    fn play(&mut self) {
        self.state = PlaybackState::Playing;

        self.player.play();
    }

    fn pause(&mut self) {
        self.state = PlaybackState::Paused;

        self.player.pause();
    }

    fn stop(&mut self) {
        self.state = PlaybackState::Stopped;

        self.player.stop();
    }

    fn get_duration(&self) -> Option<std::time::Duration> {
        self.current_duration
    }

    fn get_position(&self) -> std::time::Duration {
        self.player.get_pos()
    }

    fn seek(&mut self, seconds: i32) {
        let result = self.player.try_seek(Duration::from_secs(seconds as u64));

        match result {
            Ok(_) => {},
            Err(e) => logger::error(&format!("Error seeking: {e}")),
        }
    }

    fn set_volume(&mut self, volume: f32) {
        self.player.set_volume(volume);
    }
}