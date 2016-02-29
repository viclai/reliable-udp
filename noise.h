#ifndef NOISE_H
#define NOISE_H

/**
 * Simulate packet loss and corruption
 * @param prob[IN] the probability of packet loss or corruption
 * @return true if packet is lost or corrupted. Otherwise, false.
 */
bool simulatePacketLossCorruption(double prob);

#endif /* NOISE_H */
