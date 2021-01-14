use std::thread;
use std::time::{Duration};
use std::fs::File;

use clap::{Arg, App};
use chrono::prelude::*;
use daemonize::Daemonize;
use sysinfo::{ProcessorExt, System, SystemExt};
use serialport::{Result, SerialPort, SerialPortType, Error, ErrorKind, ClearBuffer};

fn main() {
    let matches = App::new("Gejji")
        .version("0.1.0")
        .author("Tobias Fried <friedtm@gmail.com>")
        .about("A retro readout of your system stats")
        .arg(Arg::with_name("daemonize")
            .short("d")
            .long("daemonize")
            .help("Runs the process in the background as a daemon")
            .overrides_with_all(&["quiet", "verbose"]))
        .arg(Arg::with_name("quiet")
            .short("q")
            .long("quiet")
            .alias("silent")
            .help("Does not print to stdout"))
        .arg(Arg::with_name("verbose")
            .short("v")
            .long("verbose")
            .help("Prints additional debug info to stdout")
            .conflicts_with("quiet"))
        .arg(Arg::with_name("interval")
            .short("i")
            .long("interval")
            .help("Sets update interval in seconds")
            .takes_value(true)
            .validator(|i| match i.parse::<u64>() {
                Ok(i) if i >= 1 => Ok(()),
                _ => Err("Must be number >= 1 in seconds".to_string()),
            })
            .default_value("10"))
        .get_matches();
    
    let quiet = matches.is_present("quiet");
    let verbose = matches.is_present("verbose");
    let interval = matches.value_of("interval").unwrap().parse::<u64>().unwrap();

    if matches.is_present("daemonize") {
        let stdout = File::create("/tmp/gejji.out").unwrap();
        let stderr = File::create("/tmp/gejji.err").unwrap();
        
        let daemonize = Daemonize::new()
            .pid_file("/tmp/test.pid") // Every method except `new` and `start`
            .chown_pid_file(true)      // is optional, see `Daemonize` documentation
            .working_directory("/tmp") // for default behaviour.
            .user("nobody")
            .group("daemon") // Group name
            .group(2)        // or group id.
            .umask(0o777)    // Set umask, `0o027` by default.
            .stdout(stdout)  // Redirect stdout to `/tmp/daemon.out`.
            .stderr(stderr)  // Redirect stderr to `/tmp/daemon.err`.
            .privileged_action(|| "Executed before drop privileges");

        match daemonize.start() {
            Ok(_) => println!("Success, daemonized"),
            Err(e) => eprintln!("Error, {}", e),
        }
    }

    if verbose { println!("{:?}", matches); }

    let mut sys = System::new_all();

    loop {
        sys.refresh_cpu();
        sys.refresh_memory();
        let cpu_usage = 
            (sys.get_global_processor_info().get_cpu_usage() * 10.0).round() as u64;
        let mem_usage = 
            (((sys.get_used_memory() as f64 + sys.get_total_swap() as f64)
             / sys.get_total_memory() as f64) * 1000.0)
            .round() as u64;
        

        if verbose { 
            println!("{:?}", Utc::now());
            println!("  📊 CPU...{}%", cpu_usage as f64 / 10.0);
            println!("  💾 MEM...{}%", mem_usage as f64 / 10.0);
        }

        match detect_device() {
            Ok(mut dev) => {
                dev.clear(ClearBuffer::Output).expect("Could not clear buffer");
                dev.write(format!("{}\n", cpu_usage).as_bytes()).expect("Could not write");
                dev.write(format!("{}\n", mem_usage).as_bytes()).expect("Could not write");
            },
            Err(e) => {
                if !quiet { println!("{:?}\n  {}", Utc::now(), e.description); }
            },
        }

        thread::sleep(Duration::from_secs(interval));
    }
}

fn detect_device() -> Result<Box<dyn SerialPort>> {
    if let Some(port) = serialport::available_ports()?
        .into_iter()
        .find(|p| match &p.port_type {
            SerialPortType::UsbPort(dev) =>
                dev.product.to_owned().unwrap() == "CP210x UART Bridge",
            _ => false,
        }) {
        serialport::new(port.port_name.to_owned(), 9600).open()
    } else {
        Err(Error {
            kind: ErrorKind::NoDevice,
            description: "No compatible device found".to_owned(),
        })
    }
}

