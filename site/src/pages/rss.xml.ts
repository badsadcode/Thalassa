import rss from '@astrojs/rss';
import type { APIContext } from 'astro';
import { getPosts, withBase } from '../site';

export async function GET(context: APIContext) {
  const posts = await getPosts();
  return rss({
    title: 'THALASSA devlog',
    description: 'Experiments, discoveries, systems and beautiful failures from the development of THALASSA.',
    site: context.site!,
    items: posts.map((post) => ({
      title: post.data.title,
      pubDate: post.data.date,
      description: post.data.summary,
      categories: [post.data.category],
      link: withBase(`/devlog/${post.id}/`),
    })),
  });
}
