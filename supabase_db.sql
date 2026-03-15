-- Supabase / PostgreSQL Schema for DistribPro
-- Versão 1.0

CREATE TABLE IF NOT EXISTS usuarios (
  id            SERIAL PRIMARY KEY,
  nome          VARCHAR(200) NOT NULL,
  email         VARCHAR(200) NOT NULL UNIQUE,
  senha_hash    VARCHAR(255) NOT NULL,
  role          VARCHAR(20)  NOT NULL DEFAULT 'operador',
  criado_em     TIMESTAMP    NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP    NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS fornecedores (
  id            SERIAL PRIMARY KEY,
  usuario_id    INTEGER UNIQUE REFERENCES usuarios(id) ON DELETE SET NULL,
  nome          VARCHAR(200) NOT NULL,
  cnpj          VARCHAR(20),
  contato       VARCHAR(150),
  email         VARCHAR(200) NOT NULL,
  tel           VARCHAR(30),
  categoria     VARCHAR(50),
  prazo         INTEGER,
  status        VARCHAR(20) NOT NULL DEFAULT 'Ativo',
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS produtos (
  id            SERIAL PRIMARY KEY,
  nome          VARCHAR(200) NOT NULL,
  categoria     VARCHAR(50)  NOT NULL,
  unidade       VARCHAR(30),
  preco         NUMERIC(10,2) NOT NULL,
  estoque       INTEGER NOT NULL DEFAULT 0,
  estoque_min   INTEGER NOT NULL DEFAULT 10,
  sku           VARCHAR(100) UNIQUE,
  fornecedor_id INTEGER REFERENCES fornecedores(id) ON DELETE SET NULL,
  status        VARCHAR(20) NOT NULL DEFAULT 'Ativo',
  descricao     TEXT,
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS clientes (
  id            SERIAL PRIMARY KEY,
  usuario_id    INTEGER UNIQUE REFERENCES usuarios(id) ON DELETE SET NULL,
  nome          VARCHAR(200) NOT NULL,
  tipo          CHAR(2)      NOT NULL DEFAULT 'PJ',
  doc           VARCHAR(30),
  email         VARCHAR(200) NOT NULL UNIQUE,
  tel           VARCHAR(30),
  cidade        VARCHAR(100),
  estado        CHAR(2),
  limite        NUMERIC(12,2) DEFAULT 0,
  status        VARCHAR(20) NOT NULL DEFAULT 'Ativo',
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS pedidos (
  id            SERIAL PRIMARY KEY,
  cliente_id    INTEGER NOT NULL REFERENCES clientes(id)   ON DELETE RESTRICT,
  valor         NUMERIC(12,2) NOT NULL DEFAULT 0,
  destino       VARCHAR(300),
  data_entrega  DATE,
  status        VARCHAR(20) NOT NULL DEFAULT 'Pendente',
  obs           TEXT,
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS itens_pedido (
  id            SERIAL PRIMARY KEY,
  pedido_id     INTEGER NOT NULL REFERENCES pedidos(id) ON DELETE CASCADE,
  produto_id    INTEGER NOT NULL REFERENCES produtos(id) ON DELETE RESTRICT,
  qtd           INTEGER NOT NULL,
  preco_unit    NUMERIC(10,2) NOT NULL DEFAULT 0,
  subtotal      NUMERIC(12,2) NOT NULL DEFAULT 0,
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS config_empresa (
  id            SERIAL PRIMARY KEY,
  razao_social  VARCHAR(200),
  cnpj          VARCHAR(20),
  email         VARCHAR(200),
  tel           VARCHAR(30),
  endereco      VARCHAR(300),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);
CREATE TABLE itens_pedido (
  id            SERIAL PRIMARY KEY,
  pedido_id     INTEGER NOT NULL REFERENCES pedidos(id) ON DELETE CASCADE,
  produto_id    INTEGER NOT NULL REFERENCES produtos(id) ON DELETE RESTRICT,
  qtd           INTEGER NOT NULL,
  preco_unit    NUMERIC(10,2) NOT NULL,
  subtotal      NUMERIC(12,2) NOT NULL,
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW()
);

-- Índices para performance
CREATE INDEX IF NOT EXISTS idx_pedidos_status    ON pedidos(status);
CREATE INDEX IF NOT EXISTS idx_pedidos_cliente   ON pedidos(cliente_id);
CREATE INDEX IF NOT EXISTS idx_pedidos_data      ON pedidos(data_entrega);
CREATE INDEX IF NOT EXISTS idx_produtos_estoque  ON produtos(estoque);
CREATE INDEX IF NOT EXISTS idx_itens_pedido_pedido  ON itens_pedido(pedido_id);
CREATE INDEX IF NOT EXISTS idx_itens_pedido_produto ON itens_pedido(produto_id);
CREATE INDEX IF NOT EXISTS idx_clientes_usuario     ON clientes(usuario_id);
CREATE INDEX IF NOT EXISTS idx_fornecedores_usuario  ON fornecedores(usuario_id);

-- Dados iniciais (Admin padrão)
INSERT INTO usuarios (nome, email, senha_hash, role) 
VALUES ('Admin Master', 'admin@distribpro.com.br', '$2b$12$K3y6C.E0S.uD8M.O5u.7.e5v7O8p9qA1r2s3t4u5v6w7x8y9z0a1', 'admin')
ON CONFLICT (email) DO NOTHING;
