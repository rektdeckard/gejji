use std::thread;
use std::time::Duration;
use sysinfo::{ProcessorExt, System, SystemExt};
use serialport::{Result, SerialPort, SerialPortType, Error, ErrorKind, ClearBuffer};

fn main() {
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
        
        match detect_device() {
            Ok(mut dev) => {
                dev.clear(ClearBuffer::Output).expect("Could not clear buffer");
                dev.write(format!("{}\n", cpu_usage).as_bytes()).expect("Could not write");
                dev.write(format!("{}\n", mem_usage).as_bytes()).expect("Could not write");
            },
            Err(e) => { 
                println!("{}", e.description); 
            },
        }

        thread::sleep(Duration::from_secs(10));
    }
}

fn detect_device() -> Result<Box<dyn SerialPort>> {
    if let Some(port) = serialport::available_ports()
        .unwrap()
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
