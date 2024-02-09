import paramiko
import json
import argparse




def main():
    parser = argparse.ArgumentParser(description="Description of your program")
    
    # Add arguments
    parser.add_argument("--dp_interval_peer2", type=int, help="Description of dp_interval_peer2")

    parser.add_argument("--hostname_peer1", type=str, help="SSH hostname for peer1")
    parser.add_argument("--port_peer1", type=int, default=22, help="SSH port for peer1")    
    parser.add_argument("--username_peer1", type=str, help="SSH username for peer1")
    parser.add_argument("--netshaper_dir_peer1", type=str, help="Netshaper directory for peer2") 
    
    parser.add_argument("--hostname_peer2", type=str, help="SSH hostname for peer2")
    parser.add_argument("--port_peer2", type=int, default=22, help="SSH port for peer2")
    parser.add_argument("--username_peer2", type=str, help="SSH username for peer2")
    parser.add_argument("--netshaper_dir_peer2", type=str, help="Netshaper directory for peer2") 
    parser.add_argument("--experiment_config_path", type=str, help="Path to the experiment config file")
     
    
     
    
    args = parser.parse_args()
    # check for the required arguments
    if args.dp_interval_peer2 is None:
        parser.error("Please provide the required arguments")
    else: 
        dp_interval_peer2 = args.dp_interval_peer2 
    
    # SSH details for the client-side middlebox (peer1)
    if args.hostname_peer1 is None:
        parser.error("Please provide the required arguments")
    else:
        hostname_peer1 = args.hostname_peer1
    
    if args.port_peer1 is None:
        parser.error("Please provide the required arguments")
    else:
        port_peer1 = args.port_peer1
        
    if args.username_peer1 is None:
        parser.error("Please provide the required arguments")
    else:
        username_peer1 = args.username_peer1
    if args.netshaper_dir_peer1 is None:
        parser.error("Please provide the required arguments") 
    else:
        netshaper_dir_peer1 = args.netshaper_dir_peer1 
    
         
    
    # SSH details for the server-side middlebox (peer2)
    if args.hostname_peer2 is None:
        parser.error("Please provide the required arguments")
    else:
        hostname_peer2 = args.hostname_peer2
        
    if args.port_peer2 is None:
        parser.error("Please provide the required arguments")
    else:
        port_peer2 = args.port_peer2
        
    if args.username_peer2 is None:
        parser.error("Please provide the required arguments")
    else:
        username_peer2 = args.username_peer2
    if args.netshaper_dir_peer2 is None:
        parser.error("Please provide the required arguments")     
    else:  
        netshaper_dir_peer2 = args.netshaper_dir_peer2
    if args.experiment_config_path is None:
        parser.error("Please provide the required arguments")
    else:
        experiment_config_path = args.experiment_config_path  
         
            

    experiment_config = json.load(open(experiment_config_path, 'r'))  
    
    
     
    # Peer1 JSON file path
    json_file_path_peer1 = f'{netshaper_dir_peer1}/hardware/client-middlebox/peer1_config.json'

    # Peer2 JSON file path
    json_file_path_peer2 = f'{netshaper_dir_peer2}/hardware/server-middlebox/peer2_config.json'


    # Peer1 SSH connection
    ssh_peer1 = paramiko.SSHClient()
    ssh_peer1.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh_peer1.connect(hostname_peer1, port_peer1, username=username_peer1)

    # Peer2 SSH connection
    ssh_peer2 = paramiko.SSHClient()
    ssh_peer2.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh_peer2.connect(hostname_peer2, port_peer2, username=username_peer2)
    
    
    
    # Read peer1 Config file
    stdin, stdout, stderr = ssh_peer1.exec_command('cat {}'.format(json_file_path_peer1))
    json_data_peer1 = stdout.read().decode()
    peer1_config = json.loads(json_data_peer1)
    
    # Read peer2 Config file
    stdin, stdout, stderr = ssh_peer2.exec_command('cat {}'.format(json_file_path_peer2))
    json_data_peer2 = stdout.read().decode()
    peer2_config = json.loads(json_data_peer2)

    # Modify Peer1 Config file
    peer1_config["maxClients"] = experiment_config["max_client_number"] + 2
    peer1_config["shapedClient"]["noiseMultiplier"] = experiment_config["noise_multiplier_peer1"]
    peer1_config["shapedClient"]["sensitivity"] = experiment_config["sensitivity_peer1"]
    peer1_config["shapedClient"]["DPCreditorLoopInterval"] = experiment_config["dp_interval_peer1"]
    peer1_config["shapedClient"]["sendingLoopInterval"] = experiment_config["sender_loop_interval_peer1"]
    peer1_config["shapedClient"]["maxDecisionSize"] = experiment_config["max_decision_size_peer1"]
    peer1_config["shapedClient"]["minDecisionSize"] = experiment_config["min_decision_size_peer1"]
    
    
    # Modify Peer2 Config file
    peer2_config["maxStreamsPerPeer"] = experiment_config["max_client_number"] + 2
    peer2_config["shapedServer"]["noiseMultiplier"] = experiment_config["noise_multiplier_peer2"]
    peer2_config["shapedServer"]["sensitivity"] = experiment_config["sensitivity_peer2"]
    peer2_config["shapedServer"]["DPCreditorLoopInterval"] = dp_interval_peer2 
    peer2_config["shapedServer"]["sendingLoopInterval"] = experiment_config["sender_loop_interval_peer2"]
    peer2_config["shapedServer"]["maxDecisionSize"] = experiment_config["max_decision_size_peer2"]
    peer2_config["shapedServer"]["minDecisionSize"] = experiment_config["min_decision_size_peer2"]

    
    # Write Peer1 Config file
    stdin, stdout, stderr = ssh_peer1.exec_command('echo \'{}\' > {}'.format(json.dumps(peer1_config), json_file_path_peer1))
    
    # Write Peer2 Config file
    stdin, stdout, stderr = ssh_peer2.exec_command('echo \'{}\' > {}'.format(json.dumps(peer2_config), json_file_path_peer2))

    # Close the SSH connections
    ssh_peer1.close()

    ssh_peer2.close()

    print("Fields modified successfully.")


if __name__ == "__main__":
    main()
