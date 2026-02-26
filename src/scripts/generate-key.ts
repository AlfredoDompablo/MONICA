import crypto from 'crypto';

/**
 * Generates a random secure API Key and its SHA-256 hash.
 */
function generateApiKey() {
    // 1. Generate a random 32-byte key (256 bits)
    const buffer = crypto.randomBytes(32);
    const apiKey = buffer.toString('hex'); // 64 hex characters

    // 2. Create SHA-256 hash of the key
    const hash = crypto.createHash('sha256').update(apiKey).digest('hex');

    console.log('--- New Secure API Key ---');
    console.log(`API Key (Keep secret, put in ESP32): ${apiKey}`);
    console.log(`Key Hash (Store in DB "key_hash"):   ${hash}`);
    console.log('--------------------------');
}

generateApiKey();
