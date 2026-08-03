FROM node:20-alpine
WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci

COPY . .

# Inlined into the client bundle at build time (browser calls this URL).
ARG NEXT_PUBLIC_API_URL=http://localhost:8080
ENV NEXT_PUBLIC_API_URL=$NEXT_PUBLIC_API_URL

RUN npm run build

ENV NODE_ENV=production
ENV PORT=3000
EXPOSE 3000

# Bind all interfaces so docker-compose port mapping works.
CMD ["npx", "next", "start", "-H", "0.0.0.0", "-p", "3000"]
