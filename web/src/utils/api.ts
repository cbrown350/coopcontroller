/**
 * API Authentication Utilities
 *
 * Provides authenticated fetch wrapper for HTTP Basic Authentication
 */

/**
 * Credentials storage for authenticated requests
 */
interface AuthCredentials {
  username: string
  password: string
}

let cachedCredentials: AuthCredentials | null = null

/**
 * Store authentication credentials for subsequent requests
 * @param username API username
 * @param password API password
 */
export function setAuthCredentials(username: string, password: string): void {
  cachedCredentials = { username, password }
}

/**
 * Clear stored authentication credentials
 */
export function clearAuthCredentials(): void {
  cachedCredentials = null
}

/**
 * Get stored authentication credentials
 * @returns Stored credentials or null if none
 */
export function getAuthCredentials(): AuthCredentials | null {
  return cachedCredentials
}

/**
 * Create HTTP Basic Authentication header value
 * @param username API username
 * @param password API password
 * @returns Base64-encoded credentials for Authorization header
 */
function createBasicAuthHeader(username: string, password: string): string {
  const credentials = `${username}:${password}`
  const encoded = btoa(credentials)
  return `Basic ${encoded}`
}

/**
 * Authenticated fetch wrapper for API requests
 *
 * Automatically adds HTTP Basic Authentication header if credentials are set.
 * Falls back to regular fetch if no credentials are cached.
 *
 * @param url Request URL
 * @param options Fetch options (method, headers, body, etc.)
 * @returns Promise resolving to Response object
 */
export async function authenticatedFetch(
  url: string,
  options: RequestInit = {}
): Promise<Response> {
  const finalOptions = { ...options }

  // Add authentication header if credentials are available
  if (cachedCredentials) {
    const authHeader = createBasicAuthHeader(
      cachedCredentials.username,
      cachedCredentials.password
    )

    finalOptions.headers = {
      ...finalOptions.headers,
      Authorization: authHeader,
    }
  }

  return fetch(url, finalOptions)
}

/**
 * Check if authentication credentials are currently set
 * @returns true if credentials are cached
 */
export function hasAuthCredentials(): boolean {
  return cachedCredentials !== null
}
