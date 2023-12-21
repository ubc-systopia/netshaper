#!/bin/bash

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --experiment=*)
            experiment="${arg#*=}"
            ;;
        --config_file=*)
            config_file="${arg#*=}"
            ;;
        *)
            # Unknown option
            echo "Unknown option: $arg"
            exit 1
            ;;
    esac
done

# Check if required arguments are provided
if [ -z "$experiment" ] || [ -z "$config_file" ]; then
    echo "Usage: $0 --experiment=<experiment_name> --config_file=<config_file_path>"
    exit 1
fi

# Run Python script with the provided arguments
python3 ../../simulator/simulator.py --experiment="$experiment" --config_file="$config_file"