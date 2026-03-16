# ── Stage 1: build ────────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libpq-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN make

# ── Stage 2: runtime (imagem menor, sem GCC) ──────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libpq5 \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/bin/distribpro /app/distribpro

EXPOSE 8000

CMD ["/app/distribpro"]
