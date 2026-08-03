import type { Metadata } from "next";
import { Fraunces, IBM_Plex_Mono } from "next/font/google";
import AppNav from "@/components/AppNav";
import "./globals.css";

const display = Fraunces({
  variable: "--font-display",
  subsets: ["latin"],
  axes: ["SOFT", "opsz"],
});

const mono = IBM_Plex_Mono({
  variable: "--font-mono",
  subsets: ["latin"],
  weight: ["400", "500"],
});

export const metadata: Metadata = {
  title: "Black–Scholes Pricing",
  description:
    "European option pricing UI for the Options Pricing Library C++ API",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html
      lang="en"
      className={`${display.variable} ${mono.variable} h-full antialiased`}
    >
      <body className="min-h-full">
        <AppNav />
        {children}
      </body>
    </html>
  );
}
