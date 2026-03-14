# DistribPro — Documentação Completa para Integração Backend
> Versão 1.0 | Gerado a partir do frontend existente

---

## ÍNDICE

1. [Visão Geral da Arquitetura](#1-visão-geral-da-arquitetura)
2. [Modelos de Dados (Entidades)](#2-modelos-de-dados-entidades)
3. [Rotas da API REST — Mapeamento Completo](#3-rotas-da-api-rest)
4. [Contratos de Requisição e Resposta](#4-contratos-de-requisição-e-resposta)
5. [Regras de Negócio](#5-regras-de-negócio)
6. [Autenticação e Segurança](#6-autenticação-e-segurança)
7. [Filtros, Ordenação e Paginação](#7-filtros-ordenação-e-paginação)
8. [Dashboard — Endpoints de Agregação](#8-dashboard--endpoints-de-agregação)
9. [Notificações e Alertas](#9-notificações-e-alertas)
10. [Como Integrar ao Frontend](#10-como-integrar-ao-frontend)
11. [Banco de Dados — Schema SQL Sugerido](#11-banco-de-dados--schema-sql)
12. [Stack Recomendada](#12-stack-recomendada)

---

## 1. VISÃO GERAL DA ARQUITETURA

```
┌────────────────────────────────────┐
│         FRONTEND (HTML/CSS/JS)     │
│   index.html + style.css + app.js  │
│                                    │
│  state → renderiza tabelas/gráficos│
│  CRUD → salva/edita/exclui dados   │
└───────────────┬────────────────────┘
                │  HTTP/HTTPS  (fetch / axios)
                │  JSON  ←→  JSON
                ▼
┌────────────────────────────────────┐
│          BACKEND (API REST)        │
│  C    │
│                                    │
│  /api/v1/produtos                  │
│  /api/v1/clientes                  │
│  /api/v1/fornecedores              │
│  /api/v1/pedidos                   │
│  /api/v1/dashboard                 │
│  /api/v1/auth                      │
└───────────────┬────────────────────┘
                │
                ▼
┌────────────────────────────────────┐
│          BANCO DE DADOS            │
│   PostgreSQL  OU  MySQL/MariaDB    │
└────────────────────────────────────┘
```

**Base URL da API:**  `https://api.distribpro.com.br/api/v1`

**Formato:** JSON puro em todas as requisições/respostas.

**Autenticação:** Bearer Token (JWT) no header `Authorization`.

---

## 2. MODELOS DE DADOS (ENTIDADES)

### 2.1 — Produto

Campos usados no frontend (formulário `modal-produto` e tabela `tbl-produtos`):

| Campo        | Tipo     | Obrigatório | Origem no Front       | Observação                               |
|--------------|----------|-------------|-----------------------|------------------------------------------|
| `id`         | INTEGER  | Auto        | gerado pelo backend   | PK, auto-increment                       |
| `nome`       | STRING   | ✅ Sim       | input `p-nome`        | Max 200 chars                            |
| `categoria`  | ENUM     | ✅ Sim       | select `p-categoria`  | Alimentício, Higiene, Limpeza, Bebidas, Frios, Outros |
| `unidade`    | ENUM     | Não         | select `p-unidade`    | Caixa, Fardo, Pallet, Kg, Litro, Un     |
| `preco`      | DECIMAL  | ✅ Sim       | input `p-preco`       | 2 casas decimais, maior que 0            |
| `estoque`    | INTEGER  | Não         | input `p-estoque`     | Default 0                                |
| `estoqueMin` | INTEGER  | Não         | input `p-estoque-min` | Alerta quando estoque ≤ estoqueMin       |
| `sku`        | STRING   | Não         | input `p-sku`         | Código único do produto (nullable)       |
| `fornecedor` | STRING   | Não         | select `p-fornecedor` | Nome do fornecedor (usar FK no banco)    |
| `status`     | ENUM     | Não         | select `p-status`     | Ativo (default) / Inativo                |
| `descricao`  | TEXT     | Não         | textarea `p-descricao`| Campo livre                              |
| `criadoEm`   | DATETIME | Auto        | gerado pelo backend   |                                          |
| `atualizadoEm`| DATETIME| Auto        | gerado pelo backend   |                                          |

---

### 2.2 — Cliente

| Campo    | Tipo     | Obrigatório | Origem no Front  | Observação                         |
|----------|----------|-------------|------------------|------------------------------------|
| `id`     | INTEGER  | Auto        | —                | PK                                 |
| `nome`   | STRING   | ✅ Sim       | input `c-nome`   | Razão Social ou Nome completo      |
| `tipo`   | ENUM     | Não         | select `c-tipo`  | PJ (default) / PF                  |
| `doc`    | STRING   | Não         | input `c-doc`    | CNPJ ou CPF (string com máscara)   |
| `email`  | STRING   | ✅ Sim       | input `c-email`  | Único por cliente                  |
| `tel`    | STRING   | Não         | input `c-tel`    |                                    |
| `cidade` | STRING   | Não         | input `c-cidade` |                                    |
| `estado` | CHAR(2)  | Não         | select `c-estado`| UF: SP, RJ, MG…                   |
| `limite` | DECIMAL  | Não         | input `c-limite` | Limite de crédito em reais         |
| `status` | ENUM     | Não         | select `c-status`| Ativo (default) / Inativo          |
| `criadoEm`| DATETIME| Auto        | —                |                                    |

---

### 2.3 — Fornecedor

| Campo      | Tipo     | Obrigatório | Origem no Front     | Observação                        |
|------------|----------|-------------|---------------------|-----------------------------------|
| `id`       | INTEGER  | Auto        | —                   | PK                                |
| `nome`     | STRING   | ✅ Sim       | input `f-nome`      | Razão Social                      |
| `cnpj`     | STRING   | ✅ Sim       | input `f-cnpj`      | CNPJ com máscara                  |
| `contato`  | STRING   | Não         | input `f-contato`   | Nome do responsável               |
| `email`    | STRING   | ✅ Sim       | input `f-email`     |                                   |
| `tel`      | STRING   | Não         | input `f-tel`       |                                   |
| `categoria`| ENUM     | Não         | select `f-categoria`| Alimentício, Bebidas, Limpeza, Higiene, Logística, Outros |
| `prazo`    | INTEGER  | Não         | input `f-prazo`     | Prazo de entrega em dias          |
| `status`   | ENUM     | Não         | select `f-status`   | Ativo (default) / Inativo         |
| `criadoEm` | DATETIME | Auto        | —                   |                                   |

---

### 2.4 — Pedido

| Campo     | Tipo     | Obrigatório | Origem no Front    | Observação                                       |
|-----------|----------|-------------|--------------------|-------------------------------------------------|
| `id`      | INTEGER  | Auto        | —                  | PK, exibido como `#0001`                        |
| `clienteId`| INTEGER | ✅ Sim       | select `o-cliente` | FK → clientes.id                                |
| `produtoId`| INTEGER | ✅ Sim       | select `o-produto` | FK → produtos.id                                |
| `qtd`     | INTEGER  | ✅ Sim       | input `o-qtd`      | Quantidade; deve ser ≥ 1                        |
| `valor`   | DECIMAL  | Auto        | calc `o-valor`     | Calculado: qtd × produto.preco                  |
| `destino` | STRING   | Não         | input `o-destino`  | Endereço de entrega                             |
| `data`    | DATE     | Não         | input `o-data`     | Data prevista de entrega                        |
| `status`  | ENUM     | Não         | select `o-status`  | Pendente (default), Em Rota, Entregue, Cancelado|
| `obs`     | TEXT     | Não         | textarea `o-obs`   | Observações                                     |
| `criadoEm`| DATETIME | Auto        | —                  |                                                 |

> ⚠️ **Regra importante:** Ao criar um pedido com status ≠ `Cancelado`,
> o backend DEVE decrementar `produtos.estoque -= pedido.qtd`.

---

### 2.5 — Config da Empresa

| Campo        | Tipo   | Origem no Front | Observação             |
|--------------|--------|-----------------|------------------------|
| `razaoSocial`| STRING | input `cfg-razao`|                       |
| `cnpj`       | STRING | input `cfg-cnpj` |                       |
| `email`      | STRING | input `cfg-email`|                       |
| `tel`        | STRING | input `cfg-tel`  |                       |
| `endereco`   | STRING | input `cfg-end`  |                       |

---

### 2.6 — Usuário

| Campo    | Tipo   | Origem no Front  | Observação                  |
|----------|--------|------------------|-----------------------------|
| `id`     | INTEGER| —                | PK                          |
| `nome`   | STRING | input `cfg-uname`| Exibido na sidebar e topbar |
| `email`  | STRING | input `cfg-uemail`|                            |
| `senha`  | STRING | —                | Hash bcrypt, nunca retornar |
| `role`   | ENUM   | —                | admin, operador             |

---

## 3. ROTAS DA API REST

### 3.1 Produtos

```
GET    /api/v1/produtos              → lista todos (com filtros/paginação)
GET    /api/v1/produtos/:id          → retorna um produto
POST   /api/v1/produtos              → cria novo produto
PUT    /api/v1/produtos/:id          → atualiza produto inteiro
PATCH  /api/v1/produtos/:id          → atualiza campos parciais
DELETE /api/v1/produtos/:id          → exclui produto
GET    /api/v1/produtos/estoque-baixo → produtos com estoque ≤ estoqueMin
```

### 3.2 Clientes

```
GET    /api/v1/clientes              → lista todos
GET    /api/v1/clientes/:id          → retorna um cliente
POST   /api/v1/clientes              → cria novo
PUT    /api/v1/clientes/:id          → atualiza
DELETE /api/v1/clientes/:id          → exclui
```

### 3.3 Fornecedores

```
GET    /api/v1/fornecedores          → lista todos
GET    /api/v1/fornecedores/:id      → retorna um
POST   /api/v1/fornecedores          → cria novo
PUT    /api/v1/fornecedores/:id      → atualiza
DELETE /api/v1/fornecedores/:id      → exclui
```

### 3.4 Pedidos

```
GET    /api/v1/pedidos               → lista todos
GET    /api/v1/pedidos/:id           → retorna um
POST   /api/v1/pedidos               → cria novo (DESCONTA ESTOQUE)
PUT    /api/v1/pedidos/:id           → atualiza
PATCH  /api/v1/pedidos/:id/status    → atualiza só o status
DELETE /api/v1/pedidos/:id           → exclui (REPÕE ESTOQUE se não era Cancelado)
GET    /api/v1/pedidos/recentes      → últimos 5 (para dashboard)
```

### 3.5 Dashboard

```
GET    /api/v1/dashboard/kpis        → métricas gerais
GET    /api/v1/dashboard/entregas    → volume de entregas por dia (param: dias=7|14|30)
GET    /api/v1/dashboard/status-pedidos → contagem por status
```

### 3.6 Estoque

```
GET    /api/v1/estoque               → lista produtos com posição de estoque
PATCH  /api/v1/estoque/:produtoId    → ajusta estoque manualmente
```

### 3.7 Autenticação

```
POST   /api/v1/auth/login            → login, retorna JWT
POST   /api/v1/auth/logout           → invalida token
GET    /api/v1/auth/me               → retorna dados do usuário logado
PUT    /api/v1/auth/perfil           → atualiza nome/email do usuário
PUT    /api/v1/auth/senha            → troca senha
```

### 3.8 Config

```
GET    /api/v1/config                → retorna config da empresa
PUT    /api/v1/config                → salva config da empresa
```

---

## 4. CONTRATOS DE REQUISIÇÃO E RESPOSTA

### 4.1 Padrão de Resposta

Todas as rotas devem retornar no formato:

```json
// Sucesso — lista
{
  "success": true,
  "data": [ ... ],
  "meta": {
    "total": 100,
    "page": 1,
    "perPage": 20,
    "totalPages": 5
  }
}

// Sucesso — item único
{
  "success": true,
  "data": { ... }
}

// Erro
{
  "success": false,
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Nome é obrigatório.",
    "fields": { "nome": "Campo obrigatório" }
  }
}
```

---

### 4.2 POST /api/v1/produtos

**Request body:**
```json
{
  "nome":       "Arroz Tipo 1 — Fardo 30kg",
  "categoria":  "Alimentício",
  "unidade":    "Fardo",
  "preco":      89.90,
  "estoque":    320,
  "estoqueMin": 50,
  "sku":        "ALI-ARR-030",
  "fornecedor": "Alimentos Norte S/A",
  "status":     "Ativo",
  "descricao":  "Fardo com 30 pacotes de 1kg"
}
```

**Response 201:**
```json
{
  "success": true,
  "data": {
    "id": 11,
    "nome": "Arroz Tipo 1 — Fardo 30kg",
    "categoria": "Alimentício",
    "unidade": "Fardo",
    "preco": 89.90,
    "estoque": 320,
    "estoqueMin": 50,
    "sku": "ALI-ARR-030",
    "fornecedor": "Alimentos Norte S/A",
    "status": "Ativo",
    "descricao": "Fardo com 30 pacotes de 1kg",
    "criadoEm": "2025-03-14T10:00:00Z",
    "atualizadoEm": "2025-03-14T10:00:00Z"
  }
}
```

---

### 4.3 POST /api/v1/clientes

**Request body:**
```json
{
  "nome":   "Supermercado Bom Preço Ltda",
  "tipo":   "PJ",
  "doc":    "12.345.678/0001-90",
  "email":  "compras@bompreco.com.br",
  "tel":    "(11) 3200-5500",
  "cidade": "São Paulo",
  "estado": "SP",
  "limite": 80000,
  "status": "Ativo"
}
```

**Response 201:** objeto cliente criado com `id` e timestamps.

---

### 4.4 POST /api/v1/fornecedores

**Request body:**
```json
{
  "nome":      "Alimentos Norte S/A",
  "cnpj":      "10.200.300/0001-11",
  "contato":   "Carlos Mendes",
  "email":     "vendas@alimentos-norte.com.br",
  "tel":       "(11) 3000-2200",
  "categoria": "Alimentício",
  "prazo":     5,
  "status":    "Ativo"
}
```

---

### 4.5 POST /api/v1/pedidos

**Request body:**
```json
{
  "clienteId": 1,
  "produtoId": 1,
  "qtd":       20,
  "destino":   "Av. Paulista, 1000 — SP",
  "data":      "2025-03-20",
  "status":    "Pendente",
  "obs":       ""
}
```

**Response 201:**
```json
{
  "success": true,
  "data": {
    "id": 9,
    "clienteId": 1,
    "clienteNome": "Supermercado Bom Preço Ltda",
    "produtoId": 1,
    "produtoNome": "Arroz Tipo 1 — Fardo 30kg",
    "qtd": 20,
    "valor": 1798.00,
    "destino": "Av. Paulista, 1000 — SP",
    "data": "2025-03-20",
    "status": "Pendente",
    "obs": "",
    "criadoEm": "2025-03-14T10:00:00Z"
  }
}
```

> O campo `valor` é calculado no backend: `qtd × produto.preco`
> O estoque do produto é decrementado automaticamente na criação.

---

### 4.6 GET /api/v1/dashboard/kpis

**Response 200:**
```json
{
  "success": true,
  "data": {
    "receitaTotal":        24836.00,
    "pedidosTotais":       8,
    "pedidosEmRota":       2,
    "pedidosEntregues":    3,
    "pedidosPendentes":    2,
    "pedidosCancelados":   1,
    "alertasEstoque":      2,
    "ticketMedio":         3104.50,
    "clientesAtivos":      7,
    "fornecedoresAtivos":  5,
    "totalSkus":           10
  }
}
```

---

### 4.7 GET /api/v1/dashboard/entregas?dias=7

**Response 200:**
```json
{
  "success": true,
  "data": [
    { "data": "2025-03-08", "quantidade": 34 },
    { "data": "2025-03-09", "quantidade": 41 },
    { "data": "2025-03-10", "quantidade": 28 },
    { "data": "2025-03-11", "quantidade": 55 },
    { "data": "2025-03-12", "quantidade": 62 },
    { "data": "2025-03-13", "quantidade": 47 },
    { "data": "2025-03-14", "quantidade": 38 }
  ]
}
```

---

### 4.8 POST /api/v1/auth/login

**Request body:**
```json
{
  "email": "admin@distribpro.com.br",
  "senha": "minhasenha123"
}
```

**Response 200:**
```json
{
  "success": true,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "expiresIn": 86400,
    "user": {
      "id": 1,
      "nome": "Admin Master",
      "email": "admin@distribpro.com.br",
      "role": "admin"
    }
  }
}
```

---

## 5. REGRAS DE NEGÓCIO

### 5.1 Estoque — Lógica implementada no frontend (mover para o backend)

```
// Ao CRIAR pedido (status ≠ Cancelado):
produto.estoque = produto.estoque - pedido.qtd
se produto.estoque < 0: retornar erro 422 "Estoque insuficiente"

// Ao CANCELAR pedido (PATCH status → Cancelado):
se status_anterior ≠ Cancelado:
  produto.estoque = produto.estoque + pedido.qtd

// Ao DELETAR pedido:
se pedido.status ≠ Cancelado:
  produto.estoque = produto.estoque + pedido.qtd
```

### 5.2 Alerta de Estoque Baixo

```
produto está em alerta quando: produto.estoque <= produto.estoqueMin
produto está sem estoque quando: produto.estoque === 0
```

### 5.3 Validações Obrigatórias

**Produto:**
- `nome` obrigatório, min 3 chars
- `categoria` deve ser um dos ENUMs válidos
- `preco` obrigatório, > 0

**Cliente:**
- `nome` obrigatório
- `email` obrigatório, único, formato válido

**Fornecedor:**
- `nome` obrigatório
- `email` obrigatório, formato válido

**Pedido:**
- `clienteId` obrigatório, deve existir
- `produtoId` obrigatório, deve existir
- `qtd` ≥ 1
- `qtd` ≤ `produto.estoque` (a menos que status seja Cancelado)

### 5.4 Cálculo Automático do Valor do Pedido

O frontend exibe `o-valor` como **readonly e calculado**.
O backend DEVE calcular e salvar: `valor = qtd × produto.preco`
Nunca confiar no valor enviado pelo frontend.

---

## 6. AUTENTICAÇÃO E SEGURANÇA

### Headers obrigatórios em todas as rotas protegidas

```http
Authorization: Bearer <token_jwt>
Content-Type: application/json
```

### JWT Payload

```json
{
  "sub": 1,
  "nome": "Admin Master",
  "email": "admin@distribpro.com.br",
  "role": "admin",
  "iat": 1710000000,
  "exp": 1710086400
}
```

### Proteções recomendadas

- `helmet` (Node) ou equivalente → headers de segurança
- `cors` configurado apenas para o domínio do frontend
- Rate limiting nas rotas de autenticação (máx. 10 tentativas/min)
- Senha com `bcrypt`, custo mínimo 12
- Token expira em 24h (86400 segundos)

---

## 7. FILTROS, ORDENAÇÃO E PAGINAÇÃO

```
GET /api/v1/produtos?page=1&perPage=20&sort=nome&dir=asc&status=Ativo&categoria=Bebidas&q=arroz

page      → número da página (default: 1)
perPage   → itens por página (default: 20, max: 100)
sort      → campo para ordenar (nome, preco, estoque, criadoEm)
dir       → asc | desc (default: asc)
status    → Ativo | Inativo | Pendente | Em Rota | Entregue | Cancelado
categoria → filtro de categoria
q         → busca textual
```

### Campos de busca textual por entidade

```
produtos:     nome, sku, categoria, fornecedor
clientes:     nome, email, doc, cidade
fornecedores: nome, email, cnpj, contato
pedidos:      clienteNome, produtoNome, destino, status
```

---

## 8. DASHBOARD — ENDPOINTS DE AGREGAÇÃO

| Componente               | Endpoint                                              |
|--------------------------|-------------------------------------------------------|
| KPI Cards (4 métricas)   | `GET /api/v1/dashboard/kpis`                          |
| Gráfico de Barras        | `GET /api/v1/dashboard/entregas?dias=7`               |
| Gráfico Donut            | `GET /api/v1/dashboard/status-pedidos`                |
| Tabela Pedidos Recentes  | `GET /api/v1/pedidos?sort=criadoEm&dir=desc&perPage=5`|

### Response status-pedidos

```json
{
  "success": true,
  "data": {
    "Pendente":  2,
    "Em Rota":   2,
    "Entregue":  3,
    "Cancelado": 1
  }
}
```

---

## 9. NOTIFICAÇÕES E ALERTAS

O frontend exibe ponto vermelho no sino quando há produtos com estoque crítico.

```
GET /api/v1/alertas
```

**Response 200:**
```json
{
  "success": true,
  "data": [
    {
      "tipo": "ESTOQUE_BAIXO",
      "produto": { "id": 6, "nome": "Desinfetante — Galão 5L x6", "estoque": 8, "estoqueMin": 25 }
    },
    {
      "tipo": "SEM_ESTOQUE",
      "produto": { "id": 8, "nome": "Papel Higiênico — Fardo 64un", "estoque": 0, "estoqueMin": 30 }
    }
  ],
  "meta": { "total": 2 }
}
```

---

## 10. COMO INTEGRAR AO FRONTEND

### 10.1 Configuração da API no app.js

Adicionar no topo do `app.js`:

```js
const API_BASE  = 'https://api.distribpro.com.br/api/v1';
const TOKEN_KEY = 'distribpro_token';

const getToken   = () => localStorage.getItem(TOKEN_KEY);
const setToken   = (t) => localStorage.setItem(TOKEN_KEY, t);
const clearToken = () => localStorage.removeItem(TOKEN_KEY);

const authHeader = () => ({
  'Content-Type': 'application/json',
  'Authorization': `Bearer ${getToken()}`
});

async function apiFetch(url, options = {}) {
  const res = await fetch(API_BASE + url, {
    ...options,
    headers: { ...authHeader(), ...options.headers }
  });
  if (res.status === 401) { clearToken(); location.href = '/login.html'; return; }
  return res.json();
}
```

### 10.2 Substituição do CRUD local por chamadas API

```js
// ANTES (state local)
state.produtos.push({ id: uid(), ...record });
renderTable('produtos');

// DEPOIS (API)
async function saveProduto() {
  const json = await apiFetch('/produtos', {
    method: editId ? 'PUT' : 'POST',
    body: JSON.stringify(record)
  });
  if (!json.success) { toast(json.error.message, 'error'); return; }
  toast('Produto salvo!', 'success');
  closeModal('produto');
  state.produtos = (await apiFetch('/produtos')).data;
  renderTable('produtos');
}
```

### 10.3 Carga inicial dos dados

```js
async function loadAll() {
  const [p, c, f, o] = await Promise.all([
    apiFetch('/produtos'),
    apiFetch('/clientes'),
    apiFetch('/fornecedores'),
    apiFetch('/pedidos'),
  ]);
  state.produtos     = p.data;
  state.clientes     = c.data;
  state.fornecedores = f.data;
  state.pedidos      = o.data;
}
```

---

## 11. BANCO DE DADOS — SCHEMA SQL

```sql
-- PostgreSQL / MySQL compatível

CREATE TABLE usuarios (
  id            SERIAL PRIMARY KEY,
  nome          VARCHAR(200) NOT NULL,
  email         VARCHAR(200) NOT NULL UNIQUE,
  senha_hash    VARCHAR(255) NOT NULL,
  role          VARCHAR(20)  NOT NULL DEFAULT 'operador',
  criado_em     TIMESTAMP    NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP    NOT NULL DEFAULT NOW()
);

CREATE TABLE fornecedores (
  id            SERIAL PRIMARY KEY,
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

CREATE TABLE produtos (
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

CREATE TABLE clientes (
  id            SERIAL PRIMARY KEY,
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

CREATE TABLE pedidos (
  id            SERIAL PRIMARY KEY,
  cliente_id    INTEGER NOT NULL REFERENCES clientes(id)   ON DELETE RESTRICT,
  produto_id    INTEGER NOT NULL REFERENCES produtos(id)   ON DELETE RESTRICT,
  qtd           INTEGER NOT NULL,
  valor         NUMERIC(12,2) NOT NULL,
  destino       VARCHAR(300),
  data_entrega  DATE,
  status        VARCHAR(20) NOT NULL DEFAULT 'Pendente',
  obs           TEXT,
  criado_em     TIMESTAMP NOT NULL DEFAULT NOW(),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE config_empresa (
  id            SERIAL PRIMARY KEY,
  razao_social  VARCHAR(200),
  cnpj          VARCHAR(20),
  email         VARCHAR(200),
  tel           VARCHAR(30),
  endereco      VARCHAR(300),
  atualizado_em TIMESTAMP NOT NULL DEFAULT NOW()
);

-- Índices para performance
CREATE INDEX idx_pedidos_status    ON pedidos(status);
CREATE INDEX idx_pedidos_cliente   ON pedidos(cliente_id);
CREATE INDEX idx_pedidos_data      ON pedidos(data_entrega);
CREATE INDEX idx_produtos_estoque  ON produtos(estoque);
CREATE INDEX idx_produtos_status   ON produtos(status);
CREATE INDEX idx_clientes_status   ON clientes(status);
CREATE INDEX idx_fornecedores_status ON fornecedores(status);
```

---

## 12. STACK RECOMENDADA

### Opção A — Node.js + Express (JavaScript, mais próximo do front)

```
distribpro-api/
├── src/
│   ├── routes/
│   │   ├── produtos.js
│   │   ├── clientes.js
│   │   ├── fornecedores.js
│   │   ├── pedidos.js
│   │   ├── dashboard.js
│   │   └── auth.js
│   ├── controllers/    (lógica de cada rota)
│   ├── models/         (Sequelize ou Knex)
│   ├── middleware/
│   │   ├── auth.js     (verificar JWT)
│   │   └── validate.js (Zod ou Joi)
│   ├── services/
│   │   └── estoque.js  (regras de negócio)
│   └── app.js
├── .env
└── package.json
```

**Dependências:**
```json
{
  "express": "^4.18",
  "pg": "^8.11",
  "sequelize": "^6.35",
  "jsonwebtoken": "^9.0",
  "bcrypt": "^5.1",
  "zod": "^3.22",
  "cors": "^2.8",
  "helmet": "^7.1",
  "dotenv": "^16.3"
}
```

### Opção B — Laravel (PHP)

```bash
php artisan make:model Produto -mcr
php artisan make:model Cliente -mcr
php artisan make:model Fornecedor -mcr
php artisan make:model Pedido -mcr
php artisan make:middleware CheckJwt
```

### Opção C — Django REST Framework (Python)

```bash
pip install djangorestframework djangorestframework-simplejwt
python manage.py startapp distribpro
```

---

## CHECKLIST FINAL DE IMPLEMENTAÇÃO

```
BANCO DE DADOS
[ ] Criar tabelas: usuarios, fornecedores, produtos, clientes, pedidos, config_empresa
[ ] Criar índices de performance
[ ] Popular dados iniciais (seed)

ENTIDADES — CRUD completo (×5)
[ ] GET    /api/v1/:entidade         (lista + filtros + paginação)
[ ] GET    /api/v1/:entidade/:id     (detalhe)
[ ] POST   /api/v1/:entidade         (criar com validação)
[ ] PUT    /api/v1/:entidade/:id     (atualizar completo)
[ ] DELETE /api/v1/:entidade/:id     (excluir com verificação de FK)

ROTAS ESPECIAIS
[ ] PATCH  /api/v1/pedidos/:id/status
[ ] GET    /api/v1/produtos/estoque-baixo
[ ] PATCH  /api/v1/estoque/:produtoId

DASHBOARD
[ ] GET    /api/v1/dashboard/kpis
[ ] GET    /api/v1/dashboard/entregas?dias=7
[ ] GET    /api/v1/dashboard/status-pedidos

AUTENTICAÇÃO
[ ] POST   /api/v1/auth/login
[ ] GET    /api/v1/auth/me
[ ] PUT    /api/v1/auth/perfil
[ ] PUT    /api/v1/auth/senha
[ ] Middleware JWT em todas as rotas protegidas

REGRAS DE NEGÓCIO
[ ] Decrementar estoque ao criar pedido
[ ] Repor estoque ao cancelar/deletar pedido
[ ] Calcular valor do pedido no backend (nunca confiar no front)
[ ] Retornar 422 se estoque insuficiente
[ ] Validar todos os campos obrigatórios
[ ] Hash bcrypt nas senhas (custo ≥ 12)

INTEGRAÇÃO COM FRONTEND
[ ] Adicionar API_BASE e apiFetch() no app.js
[ ] Substituir cada função CRUD do state local pela chamada fetch
[ ] Implementar loadAll() na inicialização
[ ] Tratar erros 401 (redirect para login)
[ ] Tratar erros de validação (mostrar no toast)
```
