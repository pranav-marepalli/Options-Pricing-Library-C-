# Options Pricing UI

Next.js (App Router) frontend for the C++ Black–Scholes API.

## Setup

```bash
cp .env.example .env.local
npm ci
npm run dev
```

Open [http://localhost:3000](http://localhost:3000).

`NEXT_PUBLIC_API_URL` defaults to `http://localhost:8080` (local API / docker-compose).

## Scripts

| Command | Purpose |
|---------|---------|
| `npm run dev` | Dev server on port 3000 |
| `npm run build` | Production build |
| `npm run start` | Serve production build |
| `npm run lint` | ESLint |

## Docker Compose

From the repo root (API on `:8080`, UI on `:3000`):

```bash
docker compose up --build
```

`NEXT_PUBLIC_API_URL` is passed as a build arg (default `http://localhost:8080`).

## Vercel

Set the project root to `web/` and configure `NEXT_PUBLIC_API_URL` to your deployed API URL.
