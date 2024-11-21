import matplotlib.pyplot as plt
import pandas as pd

# Load metrics data
data = pd.read_csv('metrics.txt', names=['Packet Count', 'Latency (us)', 'Throughput (KBps)'])

# Plot Latency vs Packet Count
plt.figure(figsize=(10, 5))
plt.subplot(1, 2, 1)
plt.plot(data['Packet Count'], data['Latency (us)'], label='Latency')
plt.xlabel('Packet Count')
plt.ylabel('Latency (us)')
plt.title('Latency vs Packet Count')
plt.grid(True)

# Plot Throughput vs Packet Count
plt.subplot(1, 2, 2)
plt.plot(data['Packet Count'], data['Throughput (KBps)'], label='Throughput', color='orange')
plt.xlabel('Packet Count')
plt.ylabel('Throughput (KBps)')
plt.title('Throughput vs Packet Count')
plt.grid(True)

plt.tight_layout()
plt.show()
