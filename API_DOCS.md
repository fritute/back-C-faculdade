# DistribPro API — Documentação para o Frontend

**Base URL:** `http://localhost:8000` (dev) ou a URL do Render (produção)  
**Prefixo:** `/api/v1`  
**Content-Type:** `application/json`

---

## Padrão de Respostas

**Sucesso:**
```json
{
  "success": true,
  "data": { ... }       // objeto único ou array
}
```

**Erro:**
```json
{
  "success": false,
  "error": {
    "code": "CODIGO_ERRO",
    "message": "Descrição legível"
  }
}
```

### Códigos de erro comuns

| HTTP | Código | Significado |
|------|--------|-------------|
| 400 | `BAD_REQUEST` | Body ausente ou JSON inválido |
| 400 | `VALIDATION_ERROR` | Campos obrigatórios faltando |
| 401 | `UNAUTHORIZED` | Token ausente, expirado ou credenciais inválidas |
| 404 | `NOT_FOUND` | Registro não encontrado |
| 409 | `CONFLICT` | Email duplicado (registro de usuário) |
| 500 | `DB_ERROR` | Erro interno do banco de dados |

---

## Autenticação

Todas as rotas (exceto `/auth/login` e `/auth/register`) podem exigir autenticação.  
Envie o token no header:

```
Authorization: Bearer <token>
```

O token expira em **24 horas**.

---

## 1. Auth

### POST `/api/v1/auth/register`

Cria um novo usuário e já retorna o token (auto-login).

**Body:**
```json
{
  "nome": "João Silva",         // obrigatório
  "email": "joao@email.com",    // obrigatório, único
  "senha": "minhasenha123"      // obrigatório
}
```

**Resposta (201):**
```json
{
  "success": true,
  "data": {
    "token": "aabbccdd...",
    "userId": 1,
    "nome": "João Silva",
    "role": "operador"
  }
}
```

> O role padrão é `"operador"`. Não é necessário fazer login após o registro.

---

### POST `/api/v1/auth/login`

**Body:**
```json
{
  "email": "joao@email.com",    // obrigatório
  "senha": "minhasenha123"      // obrigatório
}
```

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "token": "aabbccdd...",
    "userId": 1,
    "nome": "João Silva",
    "role": "admin"
  }
}
```

---

### POST `/api/v1/auth/logout`

Revoga o token atual. Requer `Authorization: Bearer <token>`.

**Body:** nenhum

**Resposta (200):**
```json
{
  "success": true,
  "data": { "message": "Logout realizado com sucesso." }
}
```

---

### GET `/api/v1/auth/me`

Retorna os dados do usuário logado. Requer autenticação.

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "nome": "João Silva",
    "email": "joao@email.com",
    "role": "admin",
    "criado_em": "2026-03-14 21:48:37"
  }
}
```

---

### PUT `/api/v1/auth/perfil`

Atualiza nome e email do usuário logado. Requer autenticação.

**Body:**
```json
{
  "nome": "João S. Atualizado",   // obrigatório
  "email": "novo@email.com"       // obrigatório
}
```

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "nome": "João S. Atualizado",
    "email": "novo@email.com",
    "role": "admin"
  }
}
```

---

### PUT `/api/v1/auth/senha`

Atualiza a senha do usuário logado. Requer autenticação.

**Body:**
```json
{
  "novaSenha": "novasenha456"    // obrigatório (aceita também "senha")
}
```

**Resposta (200):**
```json
{
  "success": true,
  "data": { "message": "Senha atualizada com sucesso." }
}
```

---

## 2. Produtos

### GET `/api/v1/produtos`

Lista todos os produtos.

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 1,
      "nome": "Cabo USB-C 2m",
      "categoria": "Eletrônicos",
      "unidade": "Un",
      "preco": 29.90,
      "estoque": 150,
      "estoque_min": 10,
      "sku": "SKU-001",
      "fornecedor_id": null,
      "status": "Ativo",
      "descricao": "Cabo USB-C para carregamento",
      "criado_em": "2026-03-14 21:48:37",
      "atualizado_em": "2026-03-14 21:48:37"
    }
  ]
}
```

---

### GET `/api/v1/produtos/{id}`

Retorna um produto pelo ID.

---

### POST `/api/v1/produtos`

Cria um novo produto.

**Body:**
```json
{
  "nome": "Monitor 24pol",       // obrigatório
  "categoria": "Eletrônicos",    // obrigatório
  "unidade": "Un",               // opcional (default: "Un")
  "preco": 899.90,               // opcional (default: 0)
  "estoque": 50,                 // opcional (default: 0)
  "estoque_min": 5,              // opcional (default: 10) — aceita "estoqueMin" também
  "sku": "SKU-MON-24",           // opcional
  "fornecedor_id": 1,            // opcional
  "status": "Ativo",             // opcional (default: "Ativo")
  "descricao": "Monitor Full HD" // opcional
}
```

**Resposta (201):** Produto criado com todos os campos + `id`, `criado_em`, `atualizado_em`.

---

### PUT `/api/v1/produtos/{id}`

Atualiza um produto. Mesmos campos do POST (todos opcionais no update).

**Resposta (200):** Produto atualizado.

---

### PATCH `/api/v1/produtos/{id}`

Alias para PUT (atualização parcial).

---

### DELETE `/api/v1/produtos/{id}`

Exclui um produto.

**Resposta (200):**
```json
{
  "success": true,
  "data": { "message": "Registro excluído com sucesso." }
}
```

---

### GET `/api/v1/produtos/estoque-baixo`

Lista produtos com estoque abaixo do mínimo (`estoque <= estoque_min`).

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 1,
      "nome": "Cabo USB-C",
      "estoque": 3,
      "estoque_min": 10,
      "status": "Ativo"
    }
  ]
}
```

---

## 3. Clientes

### GET `/api/v1/clientes`

Lista todos os clientes.

**Campos retornados:** `id`, `nome`, `tipo`, `doc`, `email`, `tel`, `cidade`, `estado`, `limite`, `status`, `criado_em`, `atualizado_em`

---

### GET `/api/v1/clientes/{id}`

Retorna um cliente pelo ID.

---

### POST `/api/v1/clientes`

Cria um novo cliente.

**Body:**
```json
{
  "nome": "Empresa XYZ Ltda",   // obrigatório
  "email": "contato@xyz.com",   // obrigatório
  "tipo": "PJ",                 // opcional (default: "PJ")
  "doc": "12.345.678/0001-90",  // opcional (CPF ou CNPJ)
  "tel": "(11) 99999-9999",     // opcional
  "cidade": "São Paulo",        // opcional
  "estado": "SP",               // opcional
  "limite": 50000.00,           // opcional (default: 0)
  "status": "Ativo"             // opcional (default: "Ativo")
}
```

**Resposta (201):** Cliente criado com todos os campos.

---

### PUT `/api/v1/clientes/{id}`

Atualiza um cliente. Mesmos campos do POST.

**Resposta (200):** Cliente atualizado.

---

### DELETE `/api/v1/clientes/{id}`

Exclui um cliente.

**Resposta (200):** Mensagem de sucesso.

> Não é possível excluir clientes vinculados a pedidos (erro de FK).

---

## 4. Fornecedores

### GET `/api/v1/fornecedores`

Lista todos os fornecedores.

**Campos retornados:** `id`, `nome`, `cnpj`, `contato`, `email`, `tel`, `categoria`, `prazo`, `status`, `criado_em`, `atualizado_em`

---

### GET `/api/v1/fornecedores/{id}`

Retorna um fornecedor pelo ID.

---

### POST `/api/v1/fornecedores`

Cria um novo fornecedor.

**Body:**
```json
{
  "nome": "Distribuidora ABC",  // obrigatório
  "email": "abc@forn.com",      // obrigatório
  "cnpj": "98.765.432/0001-10", // opcional
  "contato": "Carlos",          // opcional
  "tel": "(21) 3333-4444",      // opcional
  "categoria": "Eletrônicos",   // opcional
  "prazo": 15,                  // opcional (dias, default: 0)
  "status": "Ativo"             // opcional (default: "Ativo")
}
```

**Resposta (201):** Fornecedor criado com todos os campos.

---

### PUT `/api/v1/fornecedores/{id}`

Atualiza um fornecedor. Mesmos campos do POST.

---

### DELETE `/api/v1/fornecedores/{id}`

Exclui um fornecedor.

> Não é possível excluir fornecedores vinculados a produtos (FK SET NULL).

---

## 5. Pedidos

### GET `/api/v1/pedidos`

Lista todos os pedidos (inclui nome do cliente e produto via JOIN).

**Campos retornados:** `id`, `cliente_id`, `cliente_nome`, `produto_id`, `produto_nome`, `qtd`, `valor`, `destino`, `data_entrega`, `status`, `obs`, `criado_em`, `atualizado_em`

---

### GET `/api/v1/pedidos/{id}`

Retorna um pedido pelo ID (com JOINs).

---

### GET `/api/v1/pedidos/recentes`

Retorna os **5 pedidos mais recentes** (campos reduzidos).

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 10,
      "cliente_nome": "Empresa XYZ",
      "produto_nome": "Monitor 24pol",
      "qtd": 5,
      "valor": 4499.50,
      "status": "Pendente",
      "criado_em": "2026-03-14 21:48:37"
    }
  ]
}
```

---

### POST `/api/v1/pedidos`

Cria um novo pedido. **Decrementa automaticamente o estoque do produto.**

**Body:**
```json
{
  "clienteId": 1,                // obrigatório (aceita "cliente_id")
  "produtoId": 2,                // obrigatório (aceita "produto_id")
  "qtd": 10,                     // obrigatório
  "valor": 299.00,               // opcional (default: 0)
  "destino": "São Paulo - SP",   // opcional
  "data": "2026-04-01",          // opcional (data de entrega)
  "status": "Pendente",          // opcional (default: "Pendente")
  "obs": "Entregar manhã"        // opcional
}
```

**Resposta (201):** Pedido criado.

> **Importante:** O estoque é decrementado automaticamente dentro de uma transação. Se `status = "Cancelado"`, o estoque NÃO é descontado.

---

### PUT `/api/v1/pedidos/{id}`

Atualiza um pedido. Campos opcionais: `qtd`, `valor`, `destino`, `data`, `status`, `obs`.

---

### PATCH `/api/v1/pedidos/{id}/status`

Atualiza apenas o status de um pedido.

**Body:**
```json
{
  "status": "Entregue"    // obrigatório
}
```

**Status possíveis:** `Pendente`, `Em Rota`, `Entregue`, `Cancelado`

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 5,
    "status": "Entregue",
    "atualizado_em": "2026-03-14 22:00:00"
  }
}
```

---

### DELETE `/api/v1/pedidos/{id}`

Exclui um pedido. **Repõe automaticamente o estoque** se o pedido não era `Cancelado`.

---

## 6. Dashboard

### GET `/api/v1/dashboard/kpis`

Retorna indicadores gerais do sistema.

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "total_produtos": 45,
    "total_clientes": 12,
    "total_fornecedores": 8,
    "pedidos_pendentes": 3,
    "pedidos_em_rota": 5,
    "faturamento_total": 125430.50,
    "estoque_baixo": 2
  }
}
```

---

### GET `/api/v1/dashboard/entregas`

Pedidos agrupados por dia nos últimos N dias.

**Query parameter:** `?dias=7` (padrão: 7)

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    { "dia": "2026-03-10", "total": 3 },
    { "dia": "2026-03-11", "total": 5 }
  ]
}
```

---

### GET `/api/v1/dashboard/status-pedidos`

Total de pedidos agrupados por status.

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    { "status": "Entregue", "total": 42 },
    { "status": "Pendente", "total": 8 },
    { "status": "Em Rota", "total": 3 },
    { "status": "Cancelado", "total": 1 }
  ]
}
```

---

## 7. Estoque

### GET `/api/v1/estoque`

Lista todos os produtos com dados de estoque simplificados.

**Campos retornados:** `id`, `nome`, `categoria`, `estoque`, `estoque_min`, `status`

---

### PATCH `/api/v1/estoque/{id}`

Define o estoque de um produto (valor absoluto, não incremento).

**Body:**
```json
{
  "estoque": 100    // obrigatório (aceita "qtd" também)
}
```

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "nome": "Cabo USB-C",
    "estoque": 100,
    "estoque_min": 10,
    "status": "Ativo"
  }
}
```

---

## 8. Config Empresa

### GET `/api/v1/config`

Retorna dados da empresa. Retorna `"data": null` se não configurada.

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "razao_social": "DistribPro Ltda",
    "cnpj": "12.345.678/0001-90",
    "email": "contato@distribpro.com",
    "tel": "(11) 3333-4444",
    "endereco": "Rua das Flores, 123",
    "atualizado_em": "2026-03-14 22:00:00"
  }
}
```

---

### PUT `/api/v1/config`

Cria ou atualiza os dados da empresa (upsert).

**Body:**
```json
{
  "razaoSocial": "DistribPro Ltda",  // aceita "razao_social" também
  "cnpj": "12.345.678/0001-90",
  "email": "contato@distribpro.com",
  "tel": "(11) 3333-4444",
  "endereco": "Rua das Flores, 123"
}
```

**Resposta (200):** Config atualizada com todos os campos.

---

## Exemplo de uso com fetch (JavaScript)

```javascript
const API = "http://localhost:8000/api/v1";

// Login
const login = await fetch(`${API}/auth/login`, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ email: "admin@distribpro.com.br", senha: "123456" })
});
const { data } = await login.json();
const token = data.token;

// Requisição autenticada
const produtos = await fetch(`${API}/produtos`, {
  headers: { Authorization: `Bearer ${token}` }
});
const result = await produtos.json();
console.log(result.data); // array de produtos

// Criar produto
const novo = await fetch(`${API}/produtos`, {
  method: "POST",
  headers: {
    "Content-Type": "application/json",
    Authorization: `Bearer ${token}`
  },
  body: JSON.stringify({
    nome: "Teclado Mecânico",
    categoria: "Periféricos",
    preco: 349.90,
    estoque: 30
  })
});
```

---

## Mapa de Rotas Rápido

| Método | Rota | Descrição |
|--------|------|-----------|
| POST | `/auth/register` | Cadastro + auto-login |
| POST | `/auth/login` | Login |
| POST | `/auth/logout` | Logout |
| GET | `/auth/me` | Usuário logado |
| PUT | `/auth/perfil` | Atualizar perfil |
| PUT | `/auth/senha` | Alterar senha |
| GET | `/produtos` | Listar produtos |
| POST | `/produtos` | Criar produto |
| GET | `/produtos/{id}` | Produto por ID |
| PUT | `/produtos/{id}` | Atualizar produto |
| PATCH | `/produtos/{id}` | Atualizar produto (parcial) |
| DELETE | `/produtos/{id}` | Excluir produto |
| GET | `/produtos/estoque-baixo` | Produtos com estoque baixo |
| GET | `/clientes` | Listar clientes |
| POST | `/clientes` | Criar cliente |
| GET | `/clientes/{id}` | Cliente por ID |
| PUT | `/clientes/{id}` | Atualizar cliente |
| DELETE | `/clientes/{id}` | Excluir cliente |
| GET | `/fornecedores` | Listar fornecedores |
| POST | `/fornecedores` | Criar fornecedor |
| GET | `/fornecedores/{id}` | Fornecedor por ID |
| PUT | `/fornecedores/{id}` | Atualizar fornecedor |
| DELETE | `/fornecedores/{id}` | Excluir fornecedor |
| GET | `/pedidos` | Listar pedidos |
| POST | `/pedidos` | Criar pedido |
| GET | `/pedidos/{id}` | Pedido por ID |
| PUT | `/pedidos/{id}` | Atualizar pedido |
| PATCH | `/pedidos/{id}/status` | Alterar status |
| DELETE | `/pedidos/{id}` | Excluir pedido |
| GET | `/pedidos/recentes` | 5 pedidos recentes |
| GET | `/dashboard/kpis` | KPIs gerais |
| GET | `/dashboard/entregas` | Entregas por dia |
| GET | `/dashboard/status-pedidos` | Pedidos por status |
| GET | `/estoque` | Listar estoque |
| PATCH | `/estoque/{id}` | Ajustar estoque |
| GET | `/config` | Dados da empresa |
| PUT | `/config` | Atualizar empresa |
