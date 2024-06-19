import serial
import csv
from time import sleep

# Configuration
serial_port = '/dev/tty.usbmodem14401' 
baud_rate = 9600 
shots_to_capture = 1000

# File names for the CSV files
made_shots_file = 'made_shots.csv'
missed_shots_file = 'missed_shots.csv'

# Initialize serial connection
ser = serial.Serial(serial_port, baud_rate)
sleep(2)  # Wait for the connection to establish

def save_data_to_csv(file_name, data):
    """Saves captured data to a specified CSV file."""
    with open(file_name, 'a', newline='') as file:
        writer = csv.writer(file)
        for row in data:
            writer.writerow(row)

# Main data capture loop
shot_count = 0
made = 0
missed = 0

while shot_count < shots_to_capture:
    captured_data = []
    print(f"Waiting for shot {shot_count + 1} data...")

    # Wait for "SHOT START"
    while True:
        if ser.readline().decode('utf-8').strip() == "SHOT START":
            break

    # Capture data until "SHOT END"
    while True:
        line = ser.readline().decode('utf-8').strip()
        if line == "SHOT END":
            break
        captured_data.append(line.split(','))  # Assuming data is already comma-separated

    # Prompt for user input
    outcome = input("Enter shot outcome - Miss (0), Make (1), Skip (2): ").strip()
    if outcome == '0':
        save_data_to_csv(missed_shots_file, captured_data)
        print(">> DATA SAVED TO MISSED SHOTS!")
        shot_count += 1
        missed += 1
    
    elif outcome == '1':
        save_data_to_csv(made_shots_file, captured_data)
        print(">> DATA SAVED TO MADE SHOTS!")
        shot_count += 1
        made += 1
    
    elif outcome == '2':
        print("\n")
        continue  # Skip saving and wait for the next shot
    else:
        print("Invalid input. Please enter 0, 1, or 2.")
    
    accuracy = round((made/shot_count)*100, 1)
    print(f"\nAccuracy: {accuracy}%")
    print(f"Made: {made}")
    print(f"Missed: {missed}\n")

print("Data capture complete.")
