#!/usr/bin/env python3
"""
JokeAPI Scraper (Continuous Mode)
- Indefinite loop: Keeps fetching jokes forever
- No duplicates: Tracks all jokes ever fetched
- Rate limiting: Waits intelligently on 429 responses
- Batch writes: Appends new jokes to jokes.txt periodically
- Smart fetching: Tries multiple categories if one is exhausted
"""

import requests
import json
import random
import time
import os
import sys
from pathlib import Path

# Configuration
JOKEAPI_URL = "https://v2.jokeapi.dev/joke"
BLACKLIST_FLAGS = ""  # Empty = no blacklist (gets more jokes)
OUTPUT_FILE = "jokes.txt"
JOKES_PER_REQUEST = 100
REQUEST_TIMEOUT = 10
BATCH_SIZE = 10  # Write to file every N requests
JOKE_CATEGORIES = ["Any", "General", "Knock-Knock", "Programming", "Miscellaneous"]  # Fallback categories
EMPTY_RESPONSE_THRESHOLD = 20  # Switch category after N empty batches
EMPTY_BATCH_REST_TIME = 20  # Rest for N seconds when batch has no new jokes
MAX_REQUESTS_PER_MINUTE = 120  # JokeAPI rate limit: 120 requests/minute

def load_existing_jokes():
    """Load jokes already in jokes.txt to prevent duplicates"""
    if not os.path.exists(OUTPUT_FILE):
        return set()
    
    try:
        with open(OUTPUT_FILE, 'r', encoding='utf-8') as f:
            jokes = set(line.strip() for line in f if line.strip())
        print(f"📚 Loaded {len(jokes)} existing jokes from {OUTPUT_FILE}")
        return jokes
    except IOError as e:
        print(f"⚠️  Could not load existing jokes: {e}")
        return set()

def append_jokes_file(jokes):
    """Append new jokes to jokes.txt file"""
    if not jokes:
        return False
    
    try:
        with open(OUTPUT_FILE, 'a', encoding='utf-8') as f:
            for joke in jokes:
                # Escape newlines in jokes
                joke_clean = joke.replace('\n', ' ').replace('\r', '')
                f.write(joke_clean + '\n')
        return True
    except IOError as e:
        print(f"❌ Error writing file: {e}")
        return False

def get_file_stats():
    """Get current file statistics"""
    if not os.path.exists(OUTPUT_FILE):
        return 0, 0
    
    try:
        file_size = os.path.getsize(OUTPUT_FILE)
        with open(OUTPUT_FILE, 'r', encoding='utf-8') as f:
            line_count = sum(1 for _ in f)
        return file_size, line_count
    except IOError:
        return 0, 0

def preview_jokes(jokes, count=3):
    """Show a random preview of jokes"""
    if not jokes:
        return
    
    sample_size = min(count, len(jokes))
    sample = random.sample(jokes, sample_size)
    
    print(f"\n🎭 Preview ({sample_size} random new jokes):")
    print("-" * 60)
    for i, joke in enumerate(sample, 1):
        # Truncate long jokes for display
        display_joke = joke[:77] + "..." if len(joke) > 80 else joke
        print(f"{i}. {display_joke}")
    print("-" * 60)

def extract_rate_limit_info(response):
    """Extract rate limit info from response headers"""
    rate_limit = {
        'limit': response.headers.get('RateLimit-Limit', '120'),
        'remaining': response.headers.get('RateLimit-Remaining', '?'),
        'reset': response.headers.get('RateLimit-Reset', '?'),
        'retry_after': response.headers.get('Retry-After', None)
    }
    return rate_limit

def calculate_optimal_delay(remaining, limit=120, minute=60):
    """Calculate optimal delay between requests to respect rate limit
    
    Args:
        remaining: How many requests left in budget
        limit: Total requests per minute allowed
        minute: Seconds in a minute
    
    Returns:
        Delay in seconds between requests
    """
    if remaining <= 0:
        return minute  # Wait full minute if out of requests
    
    # If we have very few requests left, be more conservative
    if remaining <= 10:
        return minute / (limit / 2)  # Use half the normal rate
    
    # Normal rate: spread requests evenly across the minute
    return minute / limit

def handle_api_block(escalation_level):
    """Handle API block detection with exponential backoff
    
    Args:
        escalation_level: 0 = first block (3 min rest), 1+ = escalated (5 min rest)
    
    Returns:
        New escalation level
    """
    if escalation_level == 0:
        # First block detected: 3 minute rest
        wait_time = 180
        print(f"\n🚨 API BLOCK DETECTED - Taking 3 minute break...")
        new_level = 1
    else:
        # Already escalated: 5 minute wait
        wait_time = 300
        print(f"\n🚨 API STILL BLOCKED - Waiting 5 minutes...")
        new_level = 1
    
    for i in range(wait_time, 0, -1):
        mins = i // 60
        secs = i % 60
        print(f"  ⏳ {mins}m {secs:02d}s remaining...", end='\r')
        time.sleep(1)
    print(f"  ✅ Ready to reconnect!               \n")
    
    return new_level

def main():
    """Main continuous scraping loop"""
    # Check command-line arguments
    reset_cache = "--reset" in sys.argv
    
    print("\n" + "="*60)
    print("🤖 Starting JokeAPI Scraper (Continuous Mode)...")
    print("="*60)
    print(f"📍 Fetching from: {JOKEAPI_URL}")
    print(f"⚙️  Mode: All joke types (maximizes variety)")
    print(f"🚫 Blacklisting: None (accepts all jokes)")
    print(f"🔄 Loop: Indefinite (Ctrl+C to stop)")
    print(f"📂 Categories: {', '.join(JOKE_CATEGORIES)}")
    if reset_cache:
        print(f"🔄 Reset: Clearing cache - will re-fetch all!")
    print("="*60 + "\n")
    
    # Load existing jokes to prevent duplicates
    existing_jokes = load_existing_jokes()
    if reset_cache and os.path.exists(OUTPUT_FILE):
        print("🔄 Resetting cache...")
        os.remove(OUTPUT_FILE)
        existing_jokes = set()
        print("✅ Cache cleared!\n")
    else:
        print()
    
    session_new_jokes = []
    session_requests = 0
    session_errors = 0
    session_skipped = 0
    session_start = time.time()
    batch_count = 0
    empty_batch_count = 0
    category_index = 0
    current_category = JOKE_CATEGORIES[category_index]
    rate_limit_remaining = MAX_REQUESTS_PER_MINUTE  # Start assuming full budget
    consecutive_400_errors = 0  # Track consecutive HTTP 400 errors
    block_escalation_level = 0  # 0 = normal, 1 = escalated to 5-min waits
    
    try:
        # Continuous scraping loop
        while True:
            batch_count += 1
            print(f"📦 Batch {batch_count} (Category: {current_category})...")
            
            # Create fresh session for this batch
            session = requests.Session()
            
            # Fetch BATCH_SIZE requests before writing
            for i in range(BATCH_SIZE):
                try:
                    # Build URL with current category
                    url = f"{JOKEAPI_URL}/{current_category}"
                    
                    params = {
                        "amount": JOKES_PER_REQUEST
                        # Note: Removed type=single filter to get ALL jokes (more variety)
                        # Removed blacklistFlags (accept all content)
                    }
                    
                    session_requests += 1
                    print(f"  📡 Request #{session_requests}... ", end="", flush=True)
                    
                    response = session.get(url, params=params, timeout=REQUEST_TIMEOUT)
                    
                    # Extract rate limit info from response headers
                    rate_info = extract_rate_limit_info(response)
                    rate_limit_remaining = int(rate_info['remaining']) if rate_info['remaining'] != '?' else rate_limit_remaining - 1
                    
                    # Handle rate limiting (429 Too Many Requests)
                    if response.status_code == 429:
                        retry_after = int(rate_info['retry_after']) if rate_info['retry_after'] else 60
                        print(f"🚫 Rate limited!")
                        print(f"  ℹ️  Remaining: {rate_limit_remaining}/{rate_info['limit']}")
                        print(f"  ⏳ Waiting {retry_after} seconds (from Retry-After header)...")
                        for i in range(retry_after, 0, -1):
                            print(f"    ⏳ {i}s remaining...", end='\r')
                            time.sleep(1)
                        print(f"    ✅ Ready!                          ")
                        session_requests -= 1  # Don't count this as a real request
                        continue
                    
                    response.raise_for_status()
                    
                    data = response.json()
                    new_jokes_count = 0
                    
                    # Handle both batch and single response formats
                    if "jokes" in data:
                        for joke_obj in data["jokes"]:
                            # Extract joke text - handle both single and two-part jokes
                            if joke_obj.get("type") == "single":
                                joke_text = joke_obj.get("joke", "").strip()
                            else:
                                # Two-part joke: combine setup and delivery
                                setup = joke_obj.get("setup", "").strip()
                                delivery = joke_obj.get("delivery", "").strip()
                                joke_text = f"{setup} {delivery}".strip() if setup and delivery else ""
                            
                            if joke_text and joke_text not in existing_jokes:
                                session_new_jokes.append(joke_text)
                                existing_jokes.add(joke_text)
                                new_jokes_count += 1
                    elif "joke" in data:
                        # Single response (shouldn't happen with amount=100, but just in case)
                        if data.get("type") == "single":
                            joke_text = data.get("joke", "").strip()
                        else:
                            setup = data.get("setup", "").strip()
                            delivery = data.get("delivery", "").strip()
                            joke_text = f"{setup} {delivery}".strip() if setup and delivery else ""
                        
                        if joke_text and joke_text not in existing_jokes:
                            session_new_jokes.append(joke_text)
                            existing_jokes.add(joke_text)
                            new_jokes_count += 1
                    
                    print(f"✅ Got {new_jokes_count} new jokes (batch: {len(session_new_jokes)}, total: {len(existing_jokes)})")
                    print(f"      ℹ️  Rate limit: {rate_limit_remaining}/{rate_info['limit']} remaining")
                    
                    # Reset block detection on successful responses
                    if consecutive_400_errors > 0:
                        print(f"      ✅ Back online! Block detection reset.")
                        consecutive_400_errors = 0
                        block_escalation_level = 0
                    
                    # Calculate optimal delay based on remaining budget
                    delay = calculate_optimal_delay(rate_limit_remaining, int(rate_info['limit']))
                    time.sleep(delay)
                    
                except requests.exceptions.Timeout:
                    session_errors += 1
                    print(f"⏱️  Timeout (error #{session_errors})")
                    print(f"  ⏳ Waiting 5 seconds before retry...")
                    time.sleep(5)
                except requests.exceptions.ConnectionError:
                    session_errors += 1
                    print(f"🌐 Connection error (error #{session_errors})")
                    print(f"  ⏳ Waiting 5 seconds before retry...")
                    time.sleep(5)
                except requests.exceptions.HTTPError as e:
                    session_errors += 1
                    status_code = e.response.status_code
                    
                    # Detect API blocking (consecutive 400 errors)
                    if status_code == 400:
                        consecutive_400_errors += 1
                        print(f"❌ HTTP error {status_code} (error #{session_errors}) [400 streak: {consecutive_400_errors}]")
                        
                        # If we hit 5 consecutive 400s, API is blocking us
                        if consecutive_400_errors >= 5:
                            session.close()
                            block_escalation_level = handle_api_block(block_escalation_level)
                            session = requests.Session()  # Create new session after block recovery
                            continue  # Retry this request with new session
                        
                        print(f"  ⏳ Waiting 5 seconds before retry...")
                        time.sleep(5)
                    else:
                        # Reset 400 counter on other errors
                        consecutive_400_errors = 0
                        print(f"❌ HTTP error {status_code} (error #{session_errors})")
                        print(f"  ⏳ Waiting 5 seconds before retry...")
                        time.sleep(5)
                except Exception as e:
                    session_errors += 1
                    print(f"⚠️  Unexpected error (error #{session_errors}): {str(e)[:40]}")
                    print(f"  ⏳ Waiting 5 seconds before retry...")
                    time.sleep(5)
            
            # Close session before batch completes (fresh connection for next batch)
            session.close()
            print("\n🔌 Closed connection, preparing for next batch...\n")
            
            # Write batch to file after BATCH_SIZE requests
            if session_new_jokes:
                empty_batch_count = 0  # Reset counter on successful fetch
                print(f"\n💾 Writing {len(session_new_jokes)} new jokes to {OUTPUT_FILE}...")
                if append_jokes_file(session_new_jokes):
                    file_size, line_count = get_file_stats()
                    elapsed = time.time() - session_start
                    print(f"✅ Wrote successfully!")
                    print(f"📦 Total file size: {file_size:,} bytes ({file_size/1024:.1f} KB)")
                    print(f"📝 Total jokes in file: {line_count}")
                    print(f"⏱️  Session time: {int(elapsed)} seconds ({int(elapsed/60)}m {int(elapsed%60)}s)")
                    print(f"🔄 Total requests: {session_requests}")
                    print(f"❌ Total errors: {session_errors}")
                    print(f"🎭 Total unique jokes: {len(existing_jokes)}")
                    
                    preview_jokes(session_new_jokes, count=3)
                    
                    session_new_jokes = []  # Clear batch
                    print("\n" + "="*60)
                    print("⏳ Waiting for next batch... (Ctrl+C to stop)")
                    print("="*60 + "\n")
                else:
                    print("❌ Failed to write jokes!")
            else:
                # No new jokes in this batch
                empty_batch_count += 1
                print(f"  ℹ️  No new jokes in this batch (empty streak: {empty_batch_count}/{EMPTY_RESPONSE_THRESHOLD})")
                
                # Rest for 60 seconds whenever batch has no new jokes
                print(f"\n⏳ Batch empty - resting for {EMPTY_BATCH_REST_TIME} seconds...")
                for i in range(EMPTY_BATCH_REST_TIME, 0, -1):
                    print(f"  ⏳ {i}s remaining...", end='\r')
                    time.sleep(1)
                print(f"  ✅ Rest complete!                    \n")
                
                # Switch to next category if current one is exhausted
                if empty_batch_count >= EMPTY_RESPONSE_THRESHOLD:
                    category_index = (category_index + 1) % len(JOKE_CATEGORIES)
                    current_category = JOKE_CATEGORIES[category_index]
                    empty_batch_count = 0
                    print(f"🔄 Category exhausted! Switching to: {current_category}\n")
    
    except KeyboardInterrupt:
        print("\n\n" + "="*60)
        print("🛑 Scraper stopped by user")
        print("="*60)
        
        # Final write if any pending jokes
        if session_new_jokes:
            print(f"💾 Writing {len(session_new_jokes)} final jokes to {OUTPUT_FILE}...")
            if append_jokes_file(session_new_jokes):
                print("✅ Final batch written!")
            else:
                print("❌ Failed to write final batch")
        
        file_size, line_count = get_file_stats()
        elapsed = time.time() - session_start
        
        print(f"\n📊 Final Statistics:")
        print(f"✅ Total jokes in file: {line_count}")
        print(f"📦 File size: {file_size:,} bytes ({file_size/1024:.1f} KB)")
        print(f"⏱️  Total session time: {int(elapsed)} seconds ({int(elapsed/60)}m {int(elapsed%60)}s)")
        print(f"🔄 Total API requests: {session_requests}")
        print(f"❌ Total errors: {session_errors}")
        print(f"🎭 Total unique jokes: {len(existing_jokes)}")
        print("\n📝 Tips:")
        print("  • Run with --reset flag to clear cache and refetch all jokes")
        print("  • All joke types (single-line and two-part) are now included")
        print("  • Scraper auto-switches categories when one is exhausted")
        print("\n✨ jokes.txt is ready to upload to ESP32 LittleFS!")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\n\n❌ Fatal error: {e}")
        import traceback
        traceback.print_exc()
