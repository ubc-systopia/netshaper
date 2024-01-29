import paramiko
import json




def main():
    
    # SSH details for the client-side middlebox (peer1)
    hostname_peer1 = 'desh02'
    port_peer1 = 22
    username_peer1 = 'minesvpn'

    # SSH details for the server-side middlebox (peer2)    
    hostname_peer2 = 'desh03'
    port_peer2 = 22
    username_peer2 = 'minesvpn'
    
        
    
    # Experiment config file path
    # TODO: make it a command line argument
    experiment_file_path = "/home/minesvpn/workspace/artifact_evaluation/code/minesvpn/evaluation/video_latency/configs/video_latency.json"
    experiment_config = json.load(open(experiment_file_path, 'r'))  
    
    
     
    # Peer1 JSON file path
    json_file_path_peer1 = "~/workspace/artifact_evaluation/code/minesvpn/hardware/client-middlebox/peer1_config.json"

    # Peer2 JSON file path
    json_file_path_peer2 = "~/workspace/artifact_evaluation/code/minesvpn/hardware/server-middlebox/peer2_config.json"


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
    peer1_config["shapedClient"]["noiseMultiplier"] = experiment_config["noise_multiplier_peer1"]
    peer1_config["shapedClient"]["sensitivity"] = experiment_config["sensitivity_peer1"]
    peer1_config["shapedClient"]["DPCreditorLoopInterval"] = experiment_config["dp_interval_peer1"]
    peer1_config["shapedClient"]["sendingLoopInterval"] = experiment_config["sender_loop_interval_peer1"]
    peer1_config["shapedClient"]["maxDecisionSize"] = experiment_config["max_decision_size_peer1"]
    peer1_config["shapedClient"]["minDecisionSize"] = experiment_config["min_decision_size_peer1"]
    
    
    # Modify Peer2 Config file
    peer2_config["shapedServer"]["noiseMultiplier"] = experiment_config["noise_multiplier_peer2"]
    peer2_config["shapedServer"]["sensitivity"] = experiment_config["sensitivity_peer2"]
    peer2_config["shapedServer"]["DPCreditorLoopInterval"] = experiment_config["dp_interval_peer2"]
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
