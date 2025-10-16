-- Add enabled column to clients table, default true
ALTER TABLE clients ADD COLUMN enabled bit(1) NOT NULL DEFAULT b'1';
-- Optional index for frequent filtering
CREATE INDEX idx_clients_enabled ON clients(enabled);
