use logger;
use ui;

fn main() {
    logger::info(&format!("Welcome to Kalorite {}!", env!("CARGO_PKG_VERSION")));
    logger::info("Creating UI...");

    ui::init();

    logger::info("Done");
}
