use std::thread;
use std::time::Duration;

#[cfg(target_family = "unix")]
use std::fs::File;

#[cfg(target_family = "unix")]
use daemonize::Daemonize;

use chrono::prelude::*;
use clap::{App, Arg};
use serde_json::json;
use serialport::{Error, ErrorKind, Result, SerialPort, SerialPortType};
use sysinfo::{CpuExt, NetworkExt, NetworksExt, ProcessExt, RefreshKind, System, SystemExt};

struct Stats {
    cpu_usage: u64,
    mem_usage: u64,
    net_up_kbps: u64,
    net_down_kbps: u64,
    disk_read_kbps: u64,
    disk_write_kbps: u64,
}

fn main() {
    let matches = App::new("Gejji")
        .version("0.2.0")
        .author("Tobias Fried <friedtm@gmail.com>")
        .about("A retro readout of your system stats")
        .arg(
            Arg::with_name("daemonize")
                .short("d")
                .long("daemonize")
                .help("Runs the process in the background as a daemon")
                .overrides_with_all(&["quiet", "verbose"]),
        )
        .arg(
            Arg::with_name("quiet")
                .short("q")
                .long("quiet")
                .alias("silent")
                .help("Does not print to stdout"),
        )
        .arg(
            Arg::with_name("verbose")
                .short("v")
                .long("verbose")
                .help("Prints additional debug info to stdout")
                .conflicts_with("quiet"),
        )
        .arg(
            Arg::with_name("interval")
                .short("i")
                .long("interval")
                .help("Sets update interval in seconds")
                .takes_value(true)
                .validator(|i| match i.parse::<u64>() {
                    Ok(i) if i >= 1 => Ok(()),
                    _ => Err(String::from("Interval must be number >= 1 in seconds")),
                })
                .default_value("10"),
        )
        .arg(
            Arg::with_name("brightness")
                .short("b")
                .long("brightness")
                .help("Sets display brightness between 1-15")
                .takes_value(true)
                .validator(|i| match i.parse::<u8>() {
                    Ok(i) if i > 0 && i <= 15 => Ok(()),
                    _ => Err(String::from("Brightness must be number between 1 and 15")),
                })
                .default_value("3"),
        )
        .arg(
            Arg::with_name("oled")
                .short("o")
                .long("oled")
                .alias("oled")
                .help("Use Adafruit OLED FeatherWing display")
                .overrides_with("alphanum"),
        )
        .arg(
            Arg::with_name("alphanum")
                .short("a")
                .long("alphanum")
                .alias("alphanum")
                .help("Use Adafruit 7-segment Alphanum display")
                .overrides_with("oled"),
        )
        .get_matches();

    let quiet = matches.is_present("quiet");
    let verbose = matches.is_present("verbose");
    let interval = matches
        .value_of("interval")
        .unwrap()
        .parse::<u64>()
        .unwrap();
    let brightness = matches
        .value_of("brightness")
        .unwrap()
        .parse::<u8>()
        .unwrap();
    let alphanum = matches.is_present("alphanum");

    if verbose {
        println!("{:#?}", matches);
    }

    #[cfg(target_family = "unix")]
    if matches.is_present("daemonize") {
        let stdout = File::create("/tmp/gejji.out").unwrap();
        let stderr = File::create("/tmp/gejji.err").unwrap();

        let daemonize = Daemonize::new()
            .pid_file("/tmp/test.pid")
            .chown_pid_file(true)
            .working_directory("/tmp")
            .user("nobody")
            .group("daemon")
            .group(2)
            .umask(0o777)
            .stdout(stdout)
            .stderr(stderr)
            .privileged_action(|| "Executed before drop privileges");

        match daemonize.start() {
            Ok(_) => println!("Success, daemonized"),
            Err(e) => eprintln!("Error, {}", e),
        }
    }

    let mut sys = System::new_with_specifics(
        RefreshKind::everything()
            .without_users_list()
            .without_components(),
    );

    loop {
        let stats = get_stats(&mut sys, interval);

        if verbose {
            print_stats(&stats);
        }

        let json = serialize_message(&stats, interval, brightness, alphanum);

        match detect_device() {
            Ok(mut dev) => {
                if let Err(_) = dev.write(json.to_string().as_bytes()) {
                    println!(
                        "{:30} | Error: could not write to device",
                        format!("{:?}", Utc::now()),
                    );
                }
            }
            Err(e) => {
                if !quiet {
                    println!(
                        "{:30} | Error: {}",
                        format!("{:?}", Utc::now()),
                        e.description
                    );
                }
            }
        }

        thread::sleep(Duration::from_secs(interval));
    }
}

fn get_stats(sys: &mut System, interval: u64) -> Stats {
    sys.refresh_cpu();
    sys.refresh_memory();
    sys.refresh_networks();
    sys.refresh_processes();
    sys.refresh_disks();

    Stats {
        cpu_usage: (sys.global_cpu_info().cpu_usage() * 10.0).round() as u64,
        mem_usage: (((sys.used_memory() as f64 + sys.used_swap() as f64)
            / sys.total_memory() as f64)
            * 1000.0)
            .round() as u64,
        net_up_kbps: (sys
            .networks()
            .iter()
            .map(|(_, data)| data.transmitted())
            .sum::<u64>() as f64
            / 1024.0
            / (interval as f64))
            .round() as u64,
        net_down_kbps: (sys
            .networks()
            .iter()
            .map(|(_, data)| data.received())
            .sum::<u64>() as f64
            / 1024.0
            / (interval as f64))
            .round() as u64,
        disk_read_kbps: (sys
            .processes()
            .values()
            .map(|proc| proc.disk_usage().read_bytes)
            .sum::<u64>() as f64
            / 1024.0
            / (interval as f64))
            .round() as u64,
        disk_write_kbps: (sys
            .processes()
            .values()
            .map(|proc| proc.disk_usage().written_bytes)
            .sum::<u64>() as f64
            / 1024.0
            / (interval as f64))
            .round() as u64,
        // TODO: Debug disk IO numbers on Windows
        // #[cfg(target_family = "windows")]
        // let disk_read_kbps = (sys
        //     .processes()
        //     .values()
        //     .nth(1)
        //     .unwrap()
        //     .disk_usage()
        //     .read_bytes as f64
        //     / 1024.0
        //     / (interval as f64))
        //     .round() as u64;
        // #[cfg(target_family = "windows")]
        // let disk_write_kbps = (sys
        //     .processes()
        //     .values()
        //     .nth(1)
        //     .unwrap()
        //     .disk_usage()
        //     .written_bytes as f64
        //     / 1024.0
        //     / (interval as f64))
        //     .round() as u64;
    }
}

fn print_stats(stats: &Stats) {
    print!("{:?}", Utc::now());
    print!(" | CPU: {}%", stats.cpu_usage as f64 / 10.0);
    print!(" | MEM: {}%", stats.mem_usage as f64 / 10.0);
    print!(" | NET_UP: {}KB/s", stats.net_up_kbps);
    println!(" | NET_DN: {}KB/s", stats.net_down_kbps);
    println!(" | DISK_READ: {}KB/s", stats.disk_read_kbps);
    println!(" | DISK_WRITE: {}KB/s", stats.disk_write_kbps);
}

fn serialize_message(
    stats: &Stats,
    interval: u64,
    brightness: u8,
    alphanum: bool,
) -> serde_json::Value {
    json!({
        "cpu": stats.cpu_usage,
        "mem" : stats.mem_usage,
        "interval": interval,
        "bri": brightness,
        "alphanum": alphanum,
        "net_up": stats.net_up_kbps,
        "net_down": stats.net_down_kbps,
        "disk_read": stats.disk_read_kbps,
        "disk_write": stats.disk_write_kbps,
    })
}

fn detect_device() -> Result<Box<dyn SerialPort>> {
    if let Some(port) = serialport::available_ports()?
        .into_iter()
        .find(|p| match &p.port_type {
            SerialPortType::UsbPort(dev) => dev.product.to_owned().unwrap().contains("CP210x"),
            _ => false,
        })
    {
        serialport::new(port.port_name, 115200).open()
    } else {
        Err(Error {
            kind: ErrorKind::NoDevice,
            description: String::from("No compatible device found"),
        })
    }
}
