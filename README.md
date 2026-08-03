# Options Pricing Library

C++17 Black–Scholes pricing with Greeks and implied vol, plus a thin REST API for local Docker / Fly.io deployment and a Next.js frontend (`web/`) for Vercel.

## Library (tests / bench)

```bash
mkdir build && cd build && cmake .. && cmake --build . -j
./options_tests
./options_bench
```

## REST API

Build target `options_api` (same CMake tree). Listens on `PORT` (default `8080`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target options_api
./build/options_api
```

| Endpoint | Purpose |
|----------|---------|
| `GET /health` | Liveness |
| `POST /v1/price` | Price + Greeks |
| `POST /v1/implied-vol` | Implied volatility |

Example:

```bash
curl -s http://localhost:8080/v1/price \
  -H 'Content-Type: application/json' \
  -d '{"S0":100,"K":100,"r":0.05,"q":0.0,"sigma":0.2,"T":1.0,"type":"call"}'
```

CORS defaults to `http://localhost:3000`. Override with `CORS_ORIGIN` (e.g. your Vercel URL).

## Docker Compose (API + web)

```bash
docker compose up --build
```

- API: `http://localhost:8080`
- Web: `http://localhost:3000` with `NEXT_PUBLIC_API_URL=http://localhost:8080`

API-only image:

```bash
docker build -t options-api .
docker run --rm -p 8080:8080 -e CORS_ORIGIN=http://localhost:3000 options-api
```

## Deploy env vars

| Where | Variable | Purpose |
|-------|----------|---------|
| Vercel (`web/`) | `NEXT_PUBLIC_API_URL` | Public API base URL (e.g. `https://options-api.fly.dev`) |
| Fly (`options_api`) | `CORS_ORIGIN` | Allowed browser origin (e.g. `https://your-app.vercel.app`) |
| Fly / Docker | `PORT` | Listen port (default `8080`) |

See `fly.toml` for the API deploy stub (`fly deploy`).
