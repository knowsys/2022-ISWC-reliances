public static void main(String[] args) throws Exception {
		long start, finish;
		System.out.println("Name;Size;Tload;Tcons;Tcyc;Acyc;GRDsize\n");
		
		if (args.length < 1) {
			System.out.println("too few arguments");
			for (String arg : args)
				System.out.println(arg);
			return;
		}
		String fname = args[0];
		KBBuilder kbb = new KBBuilder();
		BufferedReader br = new BufferedReader(new FileReader(fname));
		try {
		    String line;
		    start = System.nanoTime();
		    while ((line = br.readLine()) != null) {
		    	line = line.replaceAll("[?!]", "");
		    	line = line.replaceAll("(aux-[^(]*)", "<http://www.example.org/$1>");
		       kbb.add(DlgpParser.parseRule(String.format("%s .", line)));
		    }
		    finish = System.nanoTime();
		} finally {
		    br.close();
		}
		long d0 = finish - start;
		KnowledgeBase kb = kbb.build();
		
		start = System.nanoTime();
		DefaultGraphOfRuleDependencies grd = new DefaultGraphOfRuleDependencies(kb.getOntology().iterator());
		finish = System.nanoTime();
		
		long d1 = finish - start;
		
		start = System.nanoTime();
		Boolean cyclic = grd.hasCircuit();
		finish = System.nanoTime();
		
		long d2 = finish - start;
		System.out.println(String.format("%s;%d;%.5f;%.5f;%.5f;%s;%d\n", fname, kb.getOntology().size(), (double) d0/1000000, (double) d1/1000000, (double) d2/1000000, (grd.hasCircuit() ? "no" : "yes"), grd.toString().lines().count()));
		// 8 - Close resources
		kb.close();
	}
