import { getCollection, type CollectionEntry } from 'astro:content';

// The hand-edited project status shown on the home page. Keep it honest; nothing updates it for you.
export const PROJECT_STATUS = {
  stage: 'Prototype / systems testing',
  note: 'Proving that a crew can walk around inside a moving submarine over a network. There is no playable build yet.',
};

export const REPO_URL = 'https://github.com/badsadcode/Thalassa';

export function withBase(path: string): string {
  return `${import.meta.env.BASE_URL.replace(/\/$/, '')}/${path.replace(/^\//, '')}`;
}

export type Post = CollectionEntry<'devlog'>;

/** Published posts, newest first. Same-day posts fall back to filename order, highest number first. */
export async function getPosts(): Promise<Post[]> {
  const posts = await getCollection('devlog', ({ data }) => !data.draft);
  return posts.sort(
    (a, b) => b.data.date.getTime() - a.data.date.getTime() || b.id.localeCompare(a.id),
  );
}

export function logId(post: Post): string {
  const number = post.id.match(/^(\d+)/)?.[1];
  return number ? `EXP-${number}` : 'EXP';
}

export function formatDate(date: Date): string {
  return date.toLocaleDateString('en-GB', { day: 'numeric', month: 'long', year: 'numeric', timeZone: 'UTC' });
}
