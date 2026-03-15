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
  "nome": "João Silva",         // obrigatório
  "email": "joao@email.com",    // obrigatório, único
  "senha": "minhasenha123",     // obrigatório
  "role": "cliente"             // opcional (admin, operador ou cliente. Default: operador)
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

### POST `/api/v1/auth/register-cliente`

Cria um novo usuário com role `"cliente"` e já cria o perfil de cliente vinculado. Auto-login.

**Body:**
```json
{
  "nome": "Maria Souza",        // obrigatório
  "email": "maria@email.com",   // obrigatório, único
  "senha": "minhasenha",        // obrigatório
  "tipo": "PF",                 // opcional (PF ou PJ, default: PF)
  "doc": "123.456.789-00",      // opcional
  "tel": "(11) 99999-0000",     // opcional
  "cidade": "São Paulo",        // opcional
  "estado": "SP"                // opcional
}
```

**Resposta (201):**
```json
{
  "success": true,
  "data": {
    "token": "aabbccdd...",
    "userId": 5,
    "nome": "Maria Souza",
    "role": "cliente"
  }
}
```

---

### POST `/api/v1/auth/register-fornecedor`

Cria um novo usuário com role `"fornecedor"` e já cria o perfil de fornecedor vinculado. Auto-login.

**Body:**
```json
{
  "nome": "Distribuidora ABC",  // obrigatório
  "email": "abc@dist.com",      // obrigatório, único
  "senha": "minhasenha",        // obrigatório
  "cnpj": "12.345.678/0001-90", // opcional
  "contato": "Carlos",          // opcional
  "tel": "(11) 3333-4444",      // opcional
  "categoria": "Bebidas"        // opcional
}
```

**Resposta (201):**
```json
{
  "success": true,
  "data": {
    "token": "aabbccdd...",
    "userId": 6,
    "nome": "Distribuidora ABC",
    "role": "fornecedor"
  }
}
```

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

Lista todos os pedidos com seus itens (inclui nome do cliente via JOIN). Cada pedido contém um array `itens` com os produtos.

**Campos retornados:** `id`, `cliente_id`, `cliente_nome`, `valor`, `destino`, `data_entrega`, `status`, `obs`, `criado_em`, `atualizado_em`, `itens[]`

**Campos de cada item:** `id`, `produto_id`, `produto_nome`, `qtd`, `preco_unit`, `subtotal`

---

### GET `/api/v1/pedidos/{id}`

Retorna um pedido pelo ID com seus itens.

---

### GET `/api/v1/pedidos/recentes`

Retorna os **5 pedidos mais recentes** com seus itens.

---

### POST `/api/v1/pedidos`

Cria um novo pedido com **múltiplos itens**. O valor total é calculado automaticamente a partir do preço dos produtos. **Decrementa o estoque** de cada produto dentro de uma transação.

**Body:**
```json
{
  "clienteId": 1,                // obrigatório (aceita "cliente_id")
  "itens": [                     // obrigatório, array não vazio
    { "produtoId": 2, "qtd": 10 },
    { "produtoId": 5, "qtd": 3 }
  ],
  "destino": "São Paulo - SP",   // opcional
  "data": "2026-04-01",          // opcional (data de entrega)
  "status": "Pendente",          // opcional (default: "Pendente")
  "obs": "Entregar manhã"        // opcional
}
```

**Resposta (201):** Pedido criado com itens e valor total calculado.

```json
{
  "success": true,
  "data": {
    "id": 10,
    "cliente_id": 1,
    "valor": 599.50,
    "destino": "São Paulo - SP",
    "status": "Pendente",
    "criado_em": "2026-03-14 21:48:37",
    "itens": [
      { "id": 1, "produto_id": 2, "produto_nome": "Monitor 24pol", "qtd": 10, "preco_unit": 49.95, "subtotal": 499.50 },
      { "id": 2, "produto_id": 5, "produto_nome": "Teclado USB", "qtd": 3, "preco_unit": 33.33, "subtotal": 100.00 }
    ]
  }
}
```

> **Importante:** O estoque é decrementado automaticamente para cada item. Se `status = "Cancelado"`, o estoque NÃO é descontado. O `valor` total é calculado somando os subtotais dos itens (`preco_unit * qtd`).

---

### PUT `/api/v1/pedidos/{id}`

Atualiza metadados de um pedido. Campos opcionais: `valor`, `destino`, `data`, `status`, `obs`.

> Nota: não altera os itens do pedido.

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

Exclui um pedido. **Repõe automaticamente o estoque** de todos os itens se o pedido não era `Cancelado`.

---

## 6. Meus Pedidos (Área do Cliente)

Endpoints para clientes autenticados visualizarem seus próprios pedidos. Requer autenticação com token de usuário que tenha perfil de cliente (criado via `/auth/register-cliente`).

### GET `/api/v1/meus-pedidos`

Lista todos os pedidos do cliente logado, com itens.

**Headers:** `Authorization: Bearer <token>`

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    {
      "id": 10,
      "valor": 599.50,
      "destino": "São Paulo - SP",
      "status": "Pendente",
      "criado_em": "2026-03-14 21:48:37",
      "itens": [
        { "id": 1, "produto_id": 2, "produto_nome": "Monitor 24pol", "qtd": 10, "preco_unit": 49.95, "subtotal": 499.50 }
      ]
    }
  ]
}
```

---

### GET `/api/v1/meus-pedidos/{id}`

Retorna um pedido específico do cliente logado, com itens.

**Headers:** `Authorization: Bearer <token>`

---

### GET `/api/v1/meus-pedidos/{id}/status`

Retorna apenas o status de um pedido do cliente logado.

**Headers:** `Authorization: Bearer <token>`

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "id": 10,
    "status": "Em Rota",
    "atualizado_em": "2026-03-14 22:00:00"
  }
}
```

---

## 7. Dashboard

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

## 8. Estoque

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

## 9. Config Empresa

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

## 10. Relatórios

Endpoints para gerar relatórios de vendas, estoque e financeiro. Todos aceitam query parameters `inicio` e `fim` (formato `YYYY-MM-DD`) para filtrar por período.

### GET `/api/v1/relatorios/vendas`

Resumo geral de vendas no período.

**Query params:** `?inicio=2026-01-01&fim=2026-03-31`

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "total_pedidos": 45,
    "total_itens": 120,
    "valor_total": 12500.00,
    "periodo": { "inicio": "2026-01-01", "fim": "2026-03-31" },
    "pedidos_por_status": [
      { "status": "Entregue", "total": 30, "valor": 9000.00 },
      { "status": "Pendente", "total": 15, "valor": 3500.00 }
    ]
  }
}
```

---

### GET `/api/v1/relatorios/vendas/produtos`

Ranking de produtos mais vendidos no período.

**Query params:** `?inicio=2026-01-01&fim=2026-03-31&limite=10`

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    { "produto_id": 2, "produto_nome": "Monitor 24pol", "qtd_vendida": 50, "valor_total": 24975.00 },
    { "produto_id": 5, "produto_nome": "Teclado USB", "qtd_vendida": 30, "valor_total": 2999.70 }
  ]
}
```

---

### GET `/api/v1/relatorios/vendas/clientes`

Ranking de clientes com mais vendas no período.

**Query params:** `?inicio=2026-01-01&fim=2026-03-31&limite=10`

**Resposta (200):**
```json
{
  "success": true,
  "data": [
    { "cliente_id": 1, "cliente_nome": "Empresa XYZ", "total_pedidos": 15, "valor_total": 8500.00 }
  ]
}
```

---

### GET `/api/v1/relatorios/estoque`

Relatório completo do estoque atual.

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "total_produtos": 45,
    "total_itens_estoque": 1200,
    "valor_total_estoque": 58000.00,
    "produtos_abaixo_minimo": 3,
    "produtos": [
      { "id": 1, "nome": "Cabo USB-C", "categoria": "Cabos", "estoque": 50, "estoque_min": 10, "preco": 15.90, "valor_estoque": 795.00, "status": "Ativo" }
    ]
  }
}
```

---

### GET `/api/v1/relatorios/financeiro`

Relatório financeiro com faturamento por dia.

**Query params:** `?inicio=2026-01-01&fim=2026-03-31`

**Resposta (200):**
```json
{
  "success": true,
  "data": {
    "faturamento_total": 12500.00,
    "ticket_medio": 277.78,
    "total_pedidos": 45,
    "periodo": { "inicio": "2026-01-01", "fim": "2026-03-31" },
    "faturamento_por_dia": [
      { "dia": "2026-01-15", "total_pedidos": 3, "valor": 850.00 },
      { "dia": "2026-01-16", "total_pedidos": 5, "valor": 1200.00 }
    ]
  }
}
```

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
| POST | `/auth/register` | Cadastro + auto-login (operador) |
| POST | `/auth/register-cliente` | Cadastro como cliente + auto-login |
| POST | `/auth/register-fornecedor` | Cadastro como fornecedor + auto-login |
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
| GET | `/pedidos` | Listar pedidos (com itens) |
| POST | `/pedidos` | Criar pedido (multi-item) |
| GET | `/pedidos/{id}` | Pedido por ID (com itens) |
| PUT | `/pedidos/{id}` | Atualizar pedido |
| PATCH | `/pedidos/{id}/status` | Alterar status |
| DELETE | `/pedidos/{id}` | Excluir pedido |
| GET | `/pedidos/recentes` | 5 pedidos recentes |
| GET | `/meus-pedidos` | Pedidos do cliente logado |
| GET | `/meus-pedidos/{id}` | Pedido do cliente por ID |
| GET | `/meus-pedidos/{id}/status` | Status do pedido do cliente |
| GET | `/dashboard/kpis` | KPIs gerais |
| GET | `/dashboard/entregas` | Entregas por dia |
| GET | `/dashboard/status-pedidos` | Pedidos por status |
| GET | `/estoque` | Listar estoque |
| PATCH | `/estoque/{id}` | Ajustar estoque |
| GET | `/config` | Dados da empresa |
| PUT | `/config` | Atualizar empresa |
| GET | `/relatorios/vendas` | Relatório de vendas |
| GET | `/relatorios/vendas/produtos` | Ranking produtos vendidos |
| GET | `/relatorios/vendas/clientes` | Ranking clientes |
| GET | `/relatorios/estoque` | Relatório de estoque |
| GET | `/relatorios/financeiro` | Relatório financeiro |
