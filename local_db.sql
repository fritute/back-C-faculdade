-- SQLite Backup/Local Schema for DistribPro
-- Versão 1.0

CREATE TABLE IF NOT EXISTS usuarios (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  nome          TEXT NOT NULL,
  email         TEXT NOT NULL UNIQUE,
  senha_hash    TEXT NOT NULL,
  role          TEXT NOT NULL DEFAULT 'operador',
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS fornecedores (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  usuario_id    INTEGER UNIQUE,
  nome          TEXT NOT NULL,
  cnpj          TEXT,
  contato       TEXT,
  email         TEXT NOT NULL,
  tel           TEXT,
  categoria     TEXT,
  prazo         INTEGER,
  status        TEXT NOT NULL DEFAULT 'Ativo',
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (usuario_id) REFERENCES usuarios(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS produtos (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  nome          TEXT NOT NULL,
  categoria     TEXT NOT NULL,
  unidade       TEXT,
  preco         DECIMAL(10,2) NOT NULL,
  estoque       INTEGER NOT NULL DEFAULT 0,
  estoque_min   INTEGER NOT NULL DEFAULT 10,
  sku           TEXT UNIQUE,
  fornecedor_id INTEGER,
  status        TEXT NOT NULL DEFAULT 'Ativo',
  descricao     TEXT,
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (fornecedor_id) REFERENCES fornecedores(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS clientes (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  usuario_id    INTEGER UNIQUE,
  nome          TEXT NOT NULL,
  tipo          TEXT NOT NULL DEFAULT 'PJ',
  doc           TEXT,
  email         TEXT NOT NULL UNIQUE,
  tel           TEXT,
  cidade        TEXT,
  estado        TEXT,
  limite        DECIMAL(12,2) DEFAULT 0,
  status        TEXT NOT NULL DEFAULT 'Ativo',
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (usuario_id) REFERENCES usuarios(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS pedidos (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  cliente_id    INTEGER NOT NULL,
  valor         DECIMAL(12,2) NOT NULL DEFAULT 0,
  destino       TEXT,
  data_entrega  DATE,
  status        TEXT NOT NULL DEFAULT 'Pendente',
  obs           TEXT,
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (cliente_id) REFERENCES clientes(id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS itens_pedido (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  pedido_id     INTEGER NOT NULL,
  produto_id    INTEGER NOT NULL,
  qtd           INTEGER NOT NULL,
  preco_unit    DECIMAL(10,2) NOT NULL DEFAULT 0,
  subtotal      DECIMAL(12,2) NOT NULL DEFAULT 0,
  criado_em     TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (pedido_id) REFERENCES pedidos(id) ON DELETE CASCADE,
  FOREIGN KEY (produto_id) REFERENCES produtos(id) ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS config_empresa (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  razao_social  TEXT,
  cnpj          TEXT,
  email         TEXT,
  tel           TEXT,
  endereco      TEXT,
  atualizado_em TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
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
INSERT OR IGNORE INTO usuarios (nome, email, senha_hash, role) 
VALUES ('Admin Master', 'admin@distribpro.com.br', '$2b$12$K3y6C.E0S.uD8M.O5u.7.e5v7O8p9qA1r2s3t4u5v6w7x8y9z0a1', 'admin');
