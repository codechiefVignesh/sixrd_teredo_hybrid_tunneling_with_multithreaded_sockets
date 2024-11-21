import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def plot_performance_metrics(input_file, output_prefix):
    # Read the CSV file
    df = pd.read_csv(input_file)
    
    # Set the style
    plt.style.use('ggplot')
    sns.set_palette("husl")
    
    # Create latency plot
    plt.figure(figsize=(10, 6))
    plt.plot(df['PacketSize'], df['Latency(ms)'], marker='o')
    plt.title('Latency vs Packet Size')
    plt.xlabel('Packet Size (bytes)')
    plt.ylabel('Latency (ms)')
    plt.grid(True)
    plt.savefig(f'{output_prefix}_latency.png')
    plt.close()
    
    # Create throughput plot
    plt.figure(figsize=(10, 6))
    plt.plot(df['PacketSize'], df['Throughput(Mbps)'], marker='o', color='green')
    plt.title('Throughput vs Packet Size')
    plt.xlabel('Packet Size (bytes)')
    plt.ylabel('Throughput (Mbps)')
    plt.grid(True)
    plt.savefig(f'{output_prefix}_throughput.png')
    plt.close()

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python plot_metrics.py <input_csv_file> <output_prefix>")
        sys.exit(1)
        
    plot_performance_metrics(sys.argv[1], sys.argv[2])
