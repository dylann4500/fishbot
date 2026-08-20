import type { Metadata } from "next";
import { headers } from "next/headers";
import { FishLab } from "@/components/fish-lab";

export async function generateMetadata(): Promise<Metadata> {
  const requestHeaders = await headers();
  const host = requestHeaders.get("x-forwarded-host") ?? requestHeaders.get("host") ?? "localhost:3000";
  const protocol = requestHeaders.get("x-forwarded-proto") ?? (host.startsWith("localhost") ? "http" : "https");
  const image = `${protocol}://${host}/og.png`;
  const title = "FishLab — What does optimal Fish look like?";
  const description = "Simulate Canadian Fish, compare information strategies, and replay the games that reveal the most.";
  return {
    title,
    description,
    openGraph: { title, description, type: "website", images: [{ url: image, width: 1731, height: 909, alt: "FishLab strategy simulator" }] },
    twitter: { card: "summary_large_image", title, description, images: [image] },
  };
}

export default function Home() {
  return <FishLab />;
}
