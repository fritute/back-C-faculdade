# DistribPro — Backend C

Backend completo da plataforma **DistribPro Professional** desenvolvido em linguagem C pura.  
Usa **Mongoose** como servidor HTTP, **cJSON** para JSON e **libpq** para conexão com PostgreSQL (Supabase).

---

## Stack

| Componente  | Tecnologia                              |
|-------------|-----------------------------------------|
| Linguagem   | C (C11)                                 |
| Compilador  | GCC 15.2 via MSYS2 ucrt64               |
| HTTP        | Mongoose (single-file)                  |
| JSON        | cJSON (single-file)                     |
| Banco       | PostgreSQL / Supabase (via libpq)       |
| Porta       | `8000`                                  |

---

## Estrutura do Projeto

```
src/
  main.c                    ← entry point e tabela de rotas
  db.c                      ← inicialização da conexão DB
  distribpro.h              ← header legado
  http/
    server.c                ← loop Mongoose + helpers HTTP
    handlers/
      produtos.c            ← CRUD Produtos
      clientes.c            ← CRUD Clientes
      fornecedores.c        ← CRUD Fornecedores
      pedidos.c             ← CRUD Pedidos + status
      dashboard.c           ← KPIs / entregas / status
      estoque.c             ← Controle de estoque
      auth.c                ← Login / sessões / perfil
      config.c              ← Configurações da empresa
  db/
    repo.c                  ← Todas as queries SQL (libpq)
  utils/
    common.c                ← Logging
include/distribpro/
  models.h                  ← Structs de domínio
  db.h                      ← Interface Repository + DbResult
  http.h                    ← Tipos HTTP + declarações helpers
  repo.h                    ← Repositório singleton
libs/
  mongoose.c / mongoose.h   ← Servidor HTTP embutido
  cJSON.c / cJSON.h         ← Parser JSON
compile.bat                 ← Build Windows
Makefile                    ← Build via make
supabase_db.sql             ← Schema PostgreSQL
```

---

## Pré-requisitos

1. **GCC via MSYS2** — instale o [MSYS2](https://www.msys2.org/) e rode no terminal MSYS2:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-postgresql
   ```
2. **Bibliotecas embutidas** — já estão em `libs/` (`mongoose.c`, `mongoose.h`, `cJSON.c`, `cJSON.h`).

---

## Como Compilar

### Windows (compile.bat)
```cmd
compile.bat
```

### Make
```bash
make
```

### Manual (GCC)
```bash
C:/msys64/ucrt64/bin/gcc.exe \
  src/main.c src/db.c src/http/server.c \
  src/http/handlers/produtos.c src/http/handlers/clientes.c \
  src/http/handlers/fornecedores.c src/http/handlers/pedidos.c \
  src/http/handlers/dashboard.c src/http/handlers/estoque.c \
  src/http/handlers/auth.c src/http/handlers/config.c \
  src/db/repo.c src/utils/common.c \
  libs/mongoose.c libs/cJSON.c \
  -Iinclude -Ilibs -I"C:/msys64/ucrt64/include" \
  -lws2_32 -L"C:/msys64/ucrt64/lib" -lpq \
  -o bin/distribpro.exe
```

---

## Como Executar

```cmd
bin\distribpro.exe
```

O servidor inicia em **http://0.0.0.0:8000**.

---

## Configuração do Banco de Dados

A string de conexão está em `src/main.c`. Formato:

```
host=<host> port=6543 dbname=postgres user=postgres password=<senha> sslmode=require
```

> **Supabase:** use a porta **6543** (connection pooler). A porta 5432 é bloqueada externamente.

Para criar o schema rode `supabase_db.sql` no painel SQL do Supabase.

---

## Autenticação

A API usa tokens Bearer em memória (sem JWT externo).

**Login:**
```http
POST /api/v1/auth/login
Content-Type: application/json

{ "email": "usuario@exemplo.com", "senha": "senha" }
```

**Resposta:**
```json
{
  "success": true,
  "data": {
    "token": "fe79c404f9d8a316...",
    "userId": 1,
    "nome": "Fulano",
    "role": "admin"
  }
}
```

Inclua o token nas requisições protegidas:
```
Authorization: Bearer <token>
```

Tokens expiram em **24 horas**. O servidor reiniciado invalida todos os tokens.

---

## Formato de Resposta

**Sucesso — lista:**
```json
{ "success": true, "data": [ ... ] }
```

**Sucesso — item único:**
```json
{ "success": true, "data": { ... } }
```

**Erro:**
```json
{ "success": false, "error": { "code": "NOT_FOUND", "message": "..." } }
```

---

## Referência da API

Base URL: `http://localhost:8000/api/v1`

---

### Produtos

| Método   | Endpoint                       | Descrição                        |
|----------|--------------------------------|----------------------------------|
| `GET`    | `/produtos`                    | Listar todos os produtos         |
| `POST`   | `/produtos`                    | Criar novo produto               |
| `GET`    | `/produtos/:id`                | Buscar produto por ID            |
| `PUT`    | `/produtos/:id`                | Atualizar produto completo       |
| `PATCH`  | `/produtos/:id`                | Atualizar produto parcialmente   |
| `DELETE` | `/produtos/:id`                | Excluir produto                  |
| `GET`    | `/produtos/estoque-baixo`      | Listar produtos com estoque baixo|

**Criar produto** (`POST /produtos`):
```json
{
  "nome": "Produto A",
  "sku": "SKU-001",
  "categoria": "Eletrônicos",
  "preco": 49.90,
  "estoque": 100,
  "estoque_min": 10,
  "fornecedor_id": 1,
  "status": "Ativo"
}
```

---

### Clientes

| Método   | Endpoint          | Descrição                  |
|----------|-------------------|----------------------------|
| `GET`    | `/clientes`       | Listar todos os clientes   |
| `POST`   | `/clientes`       | Criar novo cliente         |
| `GET`    | `/clientes/:id`   | Buscar cliente por ID      |
| `PUT`    | `/clientes/:id`   | Atualizar cliente          |
| `DELETE` | `/clientes/:id`   | Excluir cliente            |

**Criar cliente** (`POST /clientes`):
```json
{
  "nome": "João Silva",
  "tipo": "PF",
  "doc": "123.456.789-00",
  "email": "joao@exemplo.com",
  "tel": "(11) 99999-0000",
  "cidade": "São Paulo",
  "estado": "SP",
  "limite": 5000.00,
  "status": "Ativo"
}
```

---

### Fornecedores

| Método   | Endpoint              | Descrição                     |
|----------|-----------------------|-------------------------------|
| `GET`    | `/fornecedores`       | Listar todos os fornecedores  |
| `POST`   | `/fornecedores`       | Criar novo fornecedor         |
| `GET`    | `/fornecedores/:id`   | Buscar fornecedor por ID      |
| `PUT`    | `/fornecedores/:id`   | Atualizar fornecedor          |
| `DELETE` | `/fornecedores/:id`   | Excluir fornecedor            |

**Criar fornecedor** (`POST /fornecedores`):
```json
{
  "nome": "Distribuidora XYZ",
  "cnpj": "12.345.678/0001-99",
  "contato": "Maria",
  "email": "contato@xyz.com",
  "tel": "(11) 3000-0000",
  "categoria": "Eletrônicos",
  "prazo": 7,
  "status": "Ativo"
}
```

---

### Pedidos

| Método   | Endpoint                   | Descrição                       |
|----------|----------------------------|---------------------------------|
| `GET`    | `/pedidos`                 | Listar todos os pedidos         |
| `POST`   | `/pedidos`                 | Criar novo pedido               |
| `GET`    | `/pedidos/recentes`        | Listar pedidos recentes (10)    |
| `GET`    | `/pedidos/:id`             | Buscar pedido por ID            |
| `PUT`    | `/pedidos/:id`             | Atualizar pedido                |
| `PATCH`  | `/pedidos/:id/status`      | Atualizar apenas o status       |
| `DELETE` | `/pedidos/:id`             | Excluir pedido                  |

**Criar pedido** (`POST /pedidos`):
```json
{
  "cliente_id": 1,
  "produto_id": 1,
  "qtd": 5,
  "valor": 249.50,
  "destino": "Rua das Flores, 100 — São Paulo, SP",
  "data": "2025-06-01",
  "status": "Pendente",
  "obs": "Entregar no período da tarde"
}
```

> Ao criar um pedido, o estoque do produto é **decrementado automaticamente**.  
> Ao excluir, o estoque é **restaurado** (exceto se status for "Cancelado").

**Atualizar status** (`PATCH /pedidos/:id/status`):
```json
{ "status": "Entregue" }
```

Status válidos: `Pendente`, `Em Processamento`, `Enviado`, `Entregue`, `Cancelado`

---

### Dashboard

| Método | Endpoint                         | Descrição                          |
|--------|----------------------------------|------------------------------------|
| `GET`  | `/dashboard/kpis`                | KPIs gerais da empresa             |
| `GET`  | `/dashboard/entregas?dias=7`     | Entregas por dia (últimos N dias)  |
| `GET`  | `/dashboard/status-pedidos`      | Contagem de pedidos por status     |

**Resposta de KPIs** (`GET /dashboard/kpis`):
```json
{
  "success": true,
  "data": {
    "total_produtos": 42,
    "total_clientes": 15,
    "total_fornecedores": 8,
    "pedidos_pendentes": 3,
    "pedidos_entregues_mes": 27,
    "receita_mes": 12500.00
  }
}
```

Parâmetro `?dias=`: `7`, `14` ou `30` (padrão: `7`).

---

### Estoque

| Método  | Endpoint          | Descrição                        |
|---------|-------------------|----------------------------------|
| `GET`   | `/estoque`        | Listar situação do estoque       |
| `PATCH` | `/estoque/:id`    | Ajustar estoque de um produto    |

**Ajustar estoque** (`PATCH /estoque/:id`):
```json
{ "quantidade": 50 }
```

---

### Autenticação (`/auth`)

| Método | Endpoint         | Descrição                          |
|--------|------------------|------------------------------------|
| `POST` | `/auth/login`    | Autenticar usuário                 |
| `POST` | `/auth/logout`   | Encerrar sessão                    |
| `GET`  | `/auth/me`       | Dados do usuário autenticado       |
| `PUT`  | `/auth/perfil`   | Atualizar nome e e-mail            |
| `PUT`  | `/auth/senha`    | Alterar senha                      |

**Login** (`POST /auth/login`):
```json
{ "email": "usuario@exemplo.com", "senha": "minha_senha" }
```

**Logout** (`POST /auth/logout`):
```
Authorization: Bearer <token>
```

**Atualizar perfil** (`PUT /auth/perfil`):
```json
{ "nome": "Novo Nome", "email": "novo@email.com" }
```

**Alterar senha** (`PUT /auth/senha`):
```json
{ "senha_atual": "senha_atual", "nova_senha": "nova_senha" }
```

---

### Configurações da Empresa

| Método | Endpoint   | Descrição                              |
|--------|------------|----------------------------------------|
| `GET`  | `/config`  | Buscar configurações da empresa        |
| `PUT`  | `/config`  | Atualizar configurações da empresa     |

**Atualizar config** (`PUT /config`):
```json
{
  "razao_social": "Distribuidora DistribPro Ltda",
  "cnpj": "00.000.000/0001-00",
  "email": "contato@distribpro.com",
  "tel": "(11) 3000-0000",
  "endereco": "Av. Paulista, 1000 — São Paulo, SP"
}
```

---

## CORS

Todas as respostas incluem os headers:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
```

Requisições `OPTIONS` (preflight) retornam `204 No Content` automaticamente.

---

## Usuário de Teste

Um usuário admin foi inserido no banco para testes:

| Campo        | Valor                       |
|--------------|-----------------------------|
| `email`      | `gustavo@distribpro.com`    |
| `senha`      | `hash_teste_123`            |
| `role`       | `admin`                     |

---

## Códigos de Erro

| Código            | HTTP | Significado                      |
|-------------------|------|----------------------------------|
| `NOT_FOUND`       | 404  | Registro não encontrado          |
| `DB_ERROR`        | 500  | Erro de banco de dados           |
| `BAD_REQUEST`     | 400  | Body ausente ou JSON inválido    |
| `VALIDATION_ERROR`| 400  | Campo obrigatório faltando       |
| `UNAUTHORIZED`    | 401  | Token inválido ou expirado       |


O servidor iniciará em `http://localhost:8000`.

## Endpoints Implementados (Status Atual)

- `GET /api/v1/produtos`: Retorna uma lista de exemplo (Mock).
- Outros endpoints estão com stubs e prontos para implementação da lógica de banco de dados.
#
