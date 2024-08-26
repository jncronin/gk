d = read.table("trace.txt", sep="!")
d2 = d[seq(from=1, to=nrow(d), by=2),]
d3 = table(d2)
d4 = d3[order(d3, decreasing=TRUE)]
d5 <- sapply(rownames(d4), function(x) d$V1[match(x, d$V1)+1])
d6 <- data.frame(func=rownames(d4), src=unname(d5), cnt=as.vector(unname(d4)))
write.table(d6, file="trace.txt", row.names=FALSE, col.names=TRUE)
