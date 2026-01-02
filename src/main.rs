mod main_window;
mod player;
use gettextrs::*;

fn main() {
    setlocale(LocaleCategory::LcAll, "");
    bindtextdomain("kalorite", "./locale").unwrap();
    textdomain("kalorite").unwrap();
    let window = main_window::MainWindow::new("io.github.monsler.Kalorite");
    window.run();
}
