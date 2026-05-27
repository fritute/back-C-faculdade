-- Drop old constraints se existirem
ALTER TABLE produtos DROP CONSTRAINT IF EXISTS produtos_fornecedor_id_fkey;
ALTER TABLE pedidos DROP CONSTRAINT IF EXISTS pedidos_fornecedor_id_fkey;

-- Remove foreign key no clientes (apenas caso ja exista de users, deve apontar para usuarios)
ALTER TABLE clientes DROP CONSTRAINT IF EXISTS clientes_user_id_fkey;

-- 1. Atualizações na tabela 'produtos' (apontando fornecedor_id para usuarios)
ALTER TABLE produtos 
ADD COLUMN IF NOT EXISTS fornecedor_id INTEGER REFERENCES usuarios(id),
ADD COLUMN IF NOT EXISTS taxa_fornecedor NUMERIC DEFAULT 90,
ADD COLUMN IF NOT EXISTS taxa_operador NUMERIC DEFAULT 10,
ADD COLUMN IF NOT EXISTS img_produtos TEXT;

-- 2. Atualizações na tabela 'pedidos' (apontando fornecedor_id para usuarios)
ALTER TABLE pedidos 
ADD COLUMN IF NOT EXISTS fornecedor_id INTEGER REFERENCES usuarios(id),
ADD COLUMN IF NOT EXISTS taxa_fornecedor NUMERIC DEFAULT 90,
ADD COLUMN IF NOT EXISTS taxa_operador NUMERIC DEFAULT 10,
ADD COLUMN IF NOT EXISTS status_pagamento VARCHAR(50) DEFAULT 'Pendente';

-- 3. Atualizações na tabela 'clientes' (vincular o cliente ao seu login)
ALTER TABLE clientes 
ADD COLUMN IF NOT EXISTS user_id INTEGER REFERENCES usuarios(id);

-- Restaura as foreign keys se elas foram dropadas acima (caso a coluna ja existisse mas apontasse para fornecedores)
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM information_schema.table_constraints WHERE constraint_name='produtos_fornecedor_id_fkey') THEN
        ALTER TABLE produtos ADD CONSTRAINT produtos_fornecedor_id_fkey FOREIGN KEY (fornecedor_id) REFERENCES usuarios(id);
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.table_constraints WHERE constraint_name='pedidos_fornecedor_id_fkey') THEN
        ALTER TABLE pedidos ADD CONSTRAINT pedidos_fornecedor_id_fkey FOREIGN KEY (fornecedor_id) REFERENCES usuarios(id);
    END IF;
END $$;
