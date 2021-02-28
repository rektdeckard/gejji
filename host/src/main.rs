use std::thread;
use std::time::Duration;
use serde_json::json;

#[cfg(target_family = "unix")]
use std::fs::File;

#[cfg(target_family = "unix")]
use daemonize::Daemonize;

use chrono::prelude::*;
use clap::{App, Arg};
use serialport::{ClearBuffer, Error, ErrorKind, Result, SerialPort, SerialPortType};
use sysinfo::{ProcessorExt, System, SystemExt};

fn main() {
    let matches = App::new("Gejji")
        .version("0.1.0")
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
        .get_matches();

    let quiet = matches.is_present("quiet");
    let verbose = matches.is_present("verbose");
    let interval = matches
        .value_of("interval")
        .unwrap()
        .parse::<u64>()
        .unwrap();

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

    if verbose {
        println!("{:#?}", matches);
    }

    let mut sys = System::new_all();

    loop {
        sys.refresh_cpu();
        sys.refresh_memory();
        let cpu_usage = (sys.get_global_processor_info().get_cpu_usage() * 10.0).round() as u64;
        let mem_usage = (((sys.get_used_memory() as f64 + sys.get_total_swap() as f64)
            / sys.get_total_memory() as f64)
            * 1000.0)
            .round() as u64;

        if verbose {
            println!("{:?}", Utc::now());
            println!("  📈 CPU...{}%", cpu_usage as f64 / 10.0);
            println!("  💾 MEM...{}%", mem_usage as f64 / 10.0);
        }

        let json = json!({
            "cpu": cpu_usage,
            "mem" : mem_usage,
            "interval": interval
        });

        match detect_device() {
            Ok(mut dev) => {
                dev.clear(ClearBuffer::All).expect("Could not clear buffer");
                dev.write(format!("{}", json).as_bytes())
                    .expect("Could not write");
            }
            Err(e) => {
                if !quiet {
                    println!("{:?}\n  {}", Utc::now(), e.description);
                }
            }
        }

        thread::sleep(Duration::from_secs(interval));
    }
}

fn detect_device() -> Result<Box<dyn SerialPort>> {
    if let Some(port) = serialport::available_ports()?
        .into_iter()
        .find(|p| match &p.port_type {
            SerialPortType::UsbPort(dev) => dev.product.to_owned().unwrap().contains("CP210x"),
            _ => false,
        })
    {
        serialport::new(port.port_name, 9600).open()
    } else {
        Err(Error {
            kind: ErrorKind::NoDevice,
            description: String::from("No compatible device found"),
        })
    }
}
