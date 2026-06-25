use colored::Colorize;

/*
======================================
    Coded by Monsler at 25/06/2026
       Kalorite logging service
======================================
*/

fn get_log_str(message: &str, topic: &str) -> String {
    let current_date = chrono::Local::now().format("%Y-%m-%d %H:%M:%S");
    format!("[{} {}] {}", topic, current_date, message)
}

pub fn info(message: &str) {
    println!("{}", get_log_str(message, "INFO").bold().blue())
}

pub fn warning(message: &str) {
    println!("{}", get_log_str(message, "WARNING").bold().yellow())
}

pub fn error(message: &str) {
    println!("{}", get_log_str(message, "ERROR").bold().red())
}