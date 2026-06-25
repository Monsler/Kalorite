use std::time::Duration;
use std::path::Path;

/*
======================================
    Coded by Monsler at 25/06/2026
     Kalorite audio backend trait
======================================
*/

pub enum PlaybackState {
    Playing,
    Paused,
    Stopped
}

pub trait SoundBackend {
    fn load_track(&mut self, path: &Path) -> Result<(), String>;

    fn play(&mut self);
    fn pause(&mut self);
    fn stop(&mut self);
    fn seek(&mut self, seconds: i32);

    fn get_duration(&self) -> Option<Duration>;
    fn get_position(&self) -> Duration;

    fn set_volume(&mut self, volume: f32);
}