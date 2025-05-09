const SUPABASE_FUNCTION_URL = "https://lhdhuonairkdxzpaaqpi.supabase.co/functions/v1/monitor-firebase";
const SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxoZGh1b25haXJrZHh6cGFhcXBpIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDA2MzI1MjcsImV4cCI6MjA1NjIwODUyN30.0XliOTgjCIL66kM2f7Fpp_wQek4ALv4qNWxDmMg8c4c";

async function pollFirebase() {
  try {
    const response = await fetch(SUPABASE_FUNCTION_URL, {
      method: "GET",
      headers: { "Authorization": `Bearer ${SUPABASE_ANON_KEY}` },
    });

    if (!response.ok) {
      console.error("❌ Polling failed:", await response.text());
    } else {
      console.log("✅ Polling successful:", await response.text());
    }
  } catch (error) {
    console.error("❌ Error during polling:", error);
  }
}

// ✅ Run polling every 30 seconds
setInterval(pollFirebase, 30000);
console.log("🚀 Polling started! Running every 30 seconds...");
